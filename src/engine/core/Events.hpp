/*
===========================================================================

OTACON ENGINE
core/Events.hpp - signals and a typed event bus

The alternative to this is every system holding a pointer to every system it
needs to notify. That works until the third one, and then the audio code knows
about the player, the HUD knows about the level generator, and nothing can be
tested alone.

Two tools, because they solve different problems:

  Signal<Args...>   one publisher, many listeners, known at the call site. A
                    button's onPressed, an entity's onDied. Direct and typed.

  EventBus          many publishers, many listeners, neither knowing the other.
                    Anything can post a Died and anything can listen for one.

Both hand back a subscription handle. A listener almost always belongs to an
object with a shorter life than the signal, and unsubscribing by identity is the
only safe way to leave.

===========================================================================
*/
#pragma once
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace otacon {

using Subscription = std::uint32_t;     // 0 == invalid

/*
==================
Signal

Dispatch copies the listener list first. A listener is entitled to unsubscribe
itself -- that is the normal way an object leaves when it dies -- and mutating
the list mid-dispatch would otherwise invalidate the iteration.
==================
*/
template <typename... Args>
class Signal {
public:
    using Listener = std::function<void(Args...)>;

    Subscription connect(Listener fn) {
        const Subscription id = ++next_;
        listeners_.push_back({id, std::move(fn), true});
        return id;
    }
    void disconnect(Subscription id) {
        for (Slot& s : listeners_) if (s.id == id) { s.alive = false; return; }
    }
    void disconnectAll() { listeners_.clear(); }

    void emit(Args... args) const {
        const std::vector<Slot> snapshot = listeners_;
        for (const Slot& s : snapshot) if (s.alive && s.fn) s.fn(args...);
    }
    void operator()(Args... args) const { emit(args...); }

    std::size_t listenerCount() const {
        std::size_t n = 0;
        for (const Slot& s : listeners_) if (s.alive) ++n;
        return n;
    }
    // Drop the slots retired during dispatch. Called when convenient; not
    // required for correctness, since dead slots are skipped.
    void compact() {
        std::size_t w = 0;
        for (std::size_t r = 0; r < listeners_.size(); ++r)
            if (listeners_[r].alive) listeners_[w++] = std::move(listeners_[r]);
        listeners_.resize(w);
    }

private:
    struct Slot { Subscription id; Listener fn; bool alive; };
    mutable std::vector<Slot> listeners_;
    Subscription next_ = 0;
};

/*
==================
EventBus

Keyed by the event's own type, so there is no string name to typo and no enum to
keep in sync -- posting a type nobody listens for is simply a no-op, and
listening for a type nobody posts costs nothing.

Events are delivered immediately rather than queued. A queue would decouple
further but makes ordering hard to reason about, and at this scale the direct
call is the more debuggable choice.
==================
*/
class EventBus {
public:
    template <typename Event>
    Subscription subscribe(std::function<void(const Event&)> fn) {
        auto& channel = channelFor<Event>();
        const Subscription id = ++next_;
        channel.push_back({id, [fn = std::move(fn)](const void* p) {
            fn(*static_cast<const Event*>(p));
        }, true});
        return id;
    }

    template <typename Event>
    void post(const Event& e) const {
        const auto it = channels_.find(std::type_index(typeid(Event)));
        if (it == channels_.end()) return;
        const std::vector<Slot> snapshot = it->second;   // a handler may subscribe
        for (const Slot& s : snapshot) if (s.alive && s.fn) s.fn(&e);
    }

    void unsubscribe(Subscription id) {
        for (auto& kv : channels_)
            for (Slot& s : kv.second)
                if (s.id == id) { s.alive = false; return; }
    }
    void clear() { channels_.clear(); }

    template <typename Event>
    std::size_t listenerCount() const {
        const auto it = channels_.find(std::type_index(typeid(Event)));
        if (it == channels_.end()) return 0;
        std::size_t n = 0;
        for (const Slot& s : it->second) if (s.alive) ++n;
        return n;
    }

private:
    struct Slot { Subscription id; std::function<void(const void*)> fn; bool alive; };

    template <typename Event>
    std::vector<Slot>& channelFor() { return channels_[std::type_index(typeid(Event))]; }

    mutable std::unordered_map<std::type_index, std::vector<Slot>> channels_;
    Subscription next_ = 0;
};

} // namespace otacon
