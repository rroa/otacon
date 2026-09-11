/*
===========================================================================

OTACON ENGINE
scene/StateMachine.hpp - a finite state machine for entities and game flow

Whenever a thing can be "idle or running or jumping or dead", the alternative to
a state machine is a pile of booleans -- and booleans admit combinations that
should be impossible. `jumping && dead` is representable; a state cannot be two
states.

The contract worth reading twice is the transition order. A change does not take
effect mid-update: it is recorded and applied at a defined point. Otherwise a
state's own update() can run after its exit() has already fired, which is a
genuinely confusing class of bug -- the state that just left still ticking once.

===========================================================================
*/
#pragma once
#include "core/math/Scalar.hpp"
#include <cstddef>
#include <functional>
#include <vector>

namespace otacon {

/*
==================
State

Any of the three hooks may be left empty. `update` receives the time spent in
this state, which is what a timeout or an animation-driven exit actually needs
and saves every state from keeping its own clock.
==================
*/
template <typename Id>
struct State {
    Id id{};
    std::function<void()> enter;
    std::function<void(Real dt, float timeInState)> update;
    std::function<void()> exit;
};

template <typename Id>
class StateMachine {
public:
    // Every hook is optional. A state with none is still a state -- "Playing" in
    // a game that only needs to know it is playing is a perfectly good one, and
    // requiring a null enter() to say so would be noise.
    void add(Id id, std::function<void()> enter = {},
             std::function<void(Real, float)> update = {},
             std::function<void()> exit = {}) {
        states_.push_back({id, std::move(enter), std::move(update), std::move(exit)});
    }

    /*
    ==================
    start

    Enters without firing an exit, since there is nothing to leave. Separate
    from change() on purpose: a machine that "changes into" its first state has
    to invent a previous one.
    ==================
    */
    void start(Id id) {
        current_ = find(id);
        started_ = true;
        pending_ = false;
        elapsed_ = 0.f;
        if (current_ && current_->enter) current_->enter();
    }

    /*
    ==================
    change

    Queues a transition; update() applies it. Calling this from inside a state's
    update() is the normal case, and the deferral is what stops that state from
    being ticked again after its exit() has run.
    ==================
    */
    void change(Id id) {
        if (!started_) { start(id); return; }
        pending_ = true;
        next_ = id;
    }

    void update(Real dt) {
        applyPending();
        if (current_ && current_->update) {
            elapsed_ += toFloat(dt);
            current_->update(dt, elapsed_);
        }
        // Again after the tick, so a transition requested during update() lands
        // before anything else observes the machine.
        applyPending();
    }

    bool  running() const { return started_ && current_ != nullptr; }
    Id    state() const { return current_ ? current_->id : Id{}; }
    bool  is(Id id) const { return current_ && current_->id == id; }
    float timeInState() const { return elapsed_; }
    std::size_t stateCount() const { return states_.size(); }

private:
    State<Id>* find(Id id) {
        for (State<Id>& s : states_) if (s.id == id) return &s;
        return nullptr;
    }
    void applyPending() {
        if (!pending_) return;
        pending_ = false;
        // A state may transition to itself -- a re-trigger, like restarting an
        // attack -- so exit and enter both run rather than short-circuiting.
        if (current_ && current_->exit) current_->exit();
        current_ = find(next_);
        elapsed_ = 0.f;
        if (current_ && current_->enter) current_->enter();
    }

    std::vector<State<Id>> states_;
    State<Id>* current_ = nullptr;
    Id    next_{};
    float elapsed_ = 0.f;
    bool  started_ = false, pending_ = false;
};

} // namespace otacon
