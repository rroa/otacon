// Memory.hpp — in-house memory manager.
//
// Three allocation strategies are provided, each a classic game-engine pattern:
//
//   * TrackedHeap   general malloc-backed allocator that records every live
//                   block (size + tag + call site) so we can print a leak
//                   report and per-tag byte totals at shutdown. This is the
//                   "default" path used by makeTracked<T>/destroyTracked<T>.
//
//   * Arena         linear "bump pointer" allocator. Allocation is a pointer
//                   add; you cannot free individual blocks, only reset the
//                   whole arena (typically once per frame). O(1), zero
//                   fragmentation — ideal for transient per-frame scratch.
//
//   * PoolAllocator fixed-size free-list. Every block is the same size, so
//                   alloc/free are O(1) and there is no fragmentation — ideal
//                   for many short-lived same-type objects (gibs, particles).
//
// We deliberately do NOT override global new/delete: third-party code (GLFW)
// has its own allocations, and keeping ours explicit makes the lesson clearer.
#pragma once
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

namespace otacon {

enum class MemTag : std::uint8_t {
    General, Simulation, Render, Geometry, Particles, Debug, Count
};
const char* memTagName(MemTag t);

// ---- Tracked general-purpose heap ------------------------------------------
namespace mem {
void* alloc(std::size_t size, std::size_t align, MemTag tag, const char* site);
void  free(void* p);

struct Stats {
    std::size_t liveBytes    = 0;
    std::size_t liveBlocks   = 0;
    std::size_t peakBytes    = 0;
    std::size_t totalAllocs  = 0;
    std::size_t bytesByTag[static_cast<int>(MemTag::Count)] = {};
};
Stats stats();
void  report(const char* phase);   // prints live/peak + leaks (if any)
} // namespace mem

#define OTACON_ALLOC(size, align, tag) ::otacon::mem::alloc((size), (align), (tag), __FILE__ ":" OTACON_STR(__LINE__))
#define OTACON_STR2(x) #x
#define OTACON_STR(x) OTACON_STR2(x)

template <typename T, typename... Args>
T* makeTracked(MemTag tag, Args&&... args) {
    void* p = mem::alloc(sizeof(T), alignof(T), tag, "makeTracked<T>");
    return new (p) T(std::forward<Args>(args)...);
}
template <typename T>
void destroyTracked(T* p) {
    if (!p) return;
    p->~T();
    mem::free(p);
}

// ---- Arena (linear / bump) -------------------------------------------------
class Arena {
public:
    Arena() = default;
    explicit Arena(std::size_t bytes, MemTag tag = MemTag::General) { init(bytes, tag); }
    ~Arena();
    void  init(std::size_t bytes, MemTag tag = MemTag::General);
    void* allocate(std::size_t size, std::size_t align = alignof(std::max_align_t));
    void  reset() { offset_ = 0; }
    std::size_t used() const { return offset_; }
    std::size_t capacity() const { return capacity_; }
private:
    std::uint8_t* base_ = nullptr;
    std::size_t   capacity_ = 0;
    std::size_t   offset_ = 0;
    MemTag        tag_ = MemTag::General;
};

// ---- Pool (fixed-size free list) -------------------------------------------
class PoolAllocator {
public:
    PoolAllocator() = default;
    PoolAllocator(std::size_t blockSize, std::size_t blockCount,
                  std::size_t align = alignof(std::max_align_t), MemTag tag = MemTag::Particles) {
        init(blockSize, blockCount, align, tag);
    }
    ~PoolAllocator();
    void  init(std::size_t blockSize, std::size_t blockCount, std::size_t align, MemTag tag);
    void* allocate();        // returns nullptr when exhausted
    void  free(void* p);
    std::size_t liveBlocks() const { return live_; }
    std::size_t blockCount() const { return blockCount_; }
private:
    std::uint8_t* base_ = nullptr;
    void*         freeList_ = nullptr;
    std::size_t   blockSize_ = 0, blockCount_ = 0, live_ = 0;
    MemTag        tag_ = MemTag::Particles;
};

} // namespace otacon
