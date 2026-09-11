/*
===========================================================================

OTACON ENGINE
core/memory/Memory.cpp - in-house memory manager

Three allocators, each a classic engine pattern: a tracked heap that records
every live block so shutdown can name a leak, a bump arena for per-frame
scratch, and a fixed-size pool for many short-lived same-type objects.

Global new/delete are deliberately left alone. Third-party code (GLFW, SDL)
allocates through its own paths anyway, and keeping ours explicit is the
point - you can see at the call site which strategy a piece of code chose.

===========================================================================
*/
#include "core/memory/Memory.hpp"
#include <cstdlib>
#include <cstdio>
#include <mutex>

namespace otacon {

/*
==================
memTagName

Tag names for the leak report.
==================
*/
const char* memTagName(MemTag t) {
    switch (t) {
        case MemTag::General:    return "General";
        case MemTag::Simulation: return "Simulation";
        case MemTag::Render:     return "Render";
        case MemTag::Geometry:   return "Geometry";
        case MemTag::Particles:  return "Particles";
        case MemTag::Debug:      return "Debug";
        default:                 return "?";
    }
}

namespace mem {

// Each tracked block is prefixed with this header so free() can find its size,
// tag, and place in the doubly-linked list of live blocks.
struct alignas(std::max_align_t) Header {
    Header*     prev;
    Header*     next;
    std::size_t size;
    const char* site;
    MemTag      tag;
    std::uint32_t magic;
};
static constexpr std::uint32_t kMagic = 0xCA1AB17;

static std::mutex  g_mtx;
static Header*     g_head = nullptr;
static Stats       g_stats;

/*
==================
alloc

Over-allocate by a header, stash the size, tag and call site in front of the
block, and hand back the aligned payload. The header is what makes free()
able to account for a block without a lookup table.
==================
*/
void* alloc(std::size_t size, std::size_t align, MemTag tag, const char* site) {
    if (align < alignof(std::max_align_t)) align = alignof(std::max_align_t);
    // Reserve room for the header before the user pointer.
    std::size_t total = sizeof(Header) + size + align;
    auto* raw = static_cast<std::uint8_t*>(std::malloc(total));
    if (!raw) return nullptr;
    auto* h = reinterpret_cast<Header*>(raw);
    h->size = size; h->tag = tag; h->site = site; h->magic = kMagic;

    std::lock_guard<std::mutex> lk(g_mtx);
    h->prev = nullptr;
    h->next = g_head;
    if (g_head) g_head->prev = h;
    g_head = h;
    g_stats.liveBytes  += size;
    g_stats.liveBlocks += 1;
    g_stats.totalAllocs += 1;
    g_stats.bytesByTag[int(tag)] += size;
    if (g_stats.liveBytes > g_stats.peakBytes) g_stats.peakBytes = g_stats.liveBytes;
    return reinterpret_cast<void*>(h + 1);
}

/*
==================
free

Recover the header that alloc() placed in front of the payload and unlink
the block from the live list.
==================
*/
void free(void* p) {
    if (!p) return;
    auto* h = reinterpret_cast<Header*>(p) - 1;
    if (h->magic != kMagic) {
        std::fprintf(stderr, "[mem] free() of untracked/corrupt pointer %p\n", p);
        return;
    }
    std::lock_guard<std::mutex> lk(g_mtx);
    if (h->prev) h->prev->next = h->next; else g_head = h->next;
    if (h->next) h->next->prev = h->prev;
    g_stats.liveBytes  -= h->size;
    g_stats.liveBlocks -= 1;
    g_stats.bytesByTag[int(h->tag)] -= h->size;
    h->magic = 0;
    std::free(h);
}

/*
==================
stats

A snapshot of the live totals, for the perf overlay.
==================
*/
Stats stats() { std::lock_guard<std::mutex> lk(g_mtx); return g_stats; }

/*
==================
report

Print live and peak usage, and name anything still allocated. Called at
shutdown, where a non-empty list is a leak.
==================
*/
void report(const char* phase) {
    std::lock_guard<std::mutex> lk(g_mtx);
    std::printf("[mem] %-10s live=%zuB (%zu blocks) peak=%zuB totalAllocs=%zu\n",
                phase, g_stats.liveBytes, g_stats.liveBlocks, g_stats.peakBytes, g_stats.totalAllocs);
    for (int i = 0; i < int(MemTag::Count); ++i)
        if (g_stats.bytesByTag[i])
            std::printf("[mem]   tag %-11s %zuB\n", memTagName(MemTag(i)), g_stats.bytesByTag[i]);
    if (g_head) {
        std::printf("[mem]   LEAKS:\n");
        for (Header* h = g_head; h; h = h->next)
            std::printf("[mem]     %zuB  %-11s  %s\n", h->size, memTagName(h->tag), h->site ? h->site : "?");
    }
}

} // namespace mem

/*
=============================================================================

                                    ARENA

=============================================================================
*/

/*
==================
Arena::init

Take one block from the tracked heap and bump-allocate out of it.
==================
*/
void Arena::init(std::size_t bytes, MemTag tag) {
    base_ = static_cast<std::uint8_t*>(mem::alloc(bytes, alignof(std::max_align_t), tag, "Arena"));
    capacity_ = bytes; offset_ = 0; tag_ = tag;
}
Arena::~Arena() { if (base_) mem::free(base_); }

/*
==================
Arena::allocate

Align the offset up, then move it. There is no free: an arena is reset
wholesale, which is exactly why it is O(1) and cannot fragment.
==================
*/
void* Arena::allocate(std::size_t size, std::size_t align) {
    std::size_t cur = reinterpret_cast<std::size_t>(base_ + offset_);
    std::size_t aligned = (cur + (align - 1)) & ~(align - 1);
    std::size_t pad = aligned - cur;
    if (offset_ + pad + size > capacity_) return nullptr;   // arena full
    offset_ += pad + size;
    return base_ + (offset_ - size);
}

/*
=============================================================================

                                    POOL

=============================================================================
*/

/*
===================
PoolAllocator::init

Carve the block into a singly-linked free list, threading each node's next
pointer through the free block's own storage - a free list costs no extra
memory because it lives inside what it is tracking.
===================
*/
void PoolAllocator::init(std::size_t blockSize, std::size_t blockCount, std::size_t align, MemTag tag) {
    if (blockSize < sizeof(void*)) blockSize = sizeof(void*);   // must hold a free-list link
    blockSize = (blockSize + (align - 1)) & ~(align - 1);
    blockSize_ = blockSize; blockCount_ = blockCount; tag_ = tag; live_ = 0;
    base_ = static_cast<std::uint8_t*>(mem::alloc(blockSize * blockCount, align, tag, "Pool"));
    // Thread the free list through the raw storage.
    freeList_ = nullptr;
    for (std::size_t i = blockCount; i-- > 0; ) {
        void* blk = base_ + i * blockSize;
        *reinterpret_cast<void**>(blk) = freeList_;
        freeList_ = blk;
    }
}
PoolAllocator::~PoolAllocator() { if (base_) mem::free(base_); }

/*
=======================
PoolAllocator::allocate

Pop the head of the free list. Returns null when exhausted rather than
growing: a pool's fixed size is a budget you are meant to notice.
=======================
*/
void* PoolAllocator::allocate() {
    if (!freeList_) return nullptr;
    void* blk = freeList_;
    freeList_ = *reinterpret_cast<void**>(blk);
    ++live_;
    return blk;
}

/*
===================
PoolAllocator::free

Push the block back onto the free list.
===================
*/
void PoolAllocator::free(void* p) {
    if (!p) return;
    *reinterpret_cast<void**>(p) = freeList_;
    freeList_ = p;
    --live_;
}

} // namespace otacon
