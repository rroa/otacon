/*
===========================================================================

OTACON ENGINE
scene/Entity.cpp - physics-enabled scene body

The engine's FlxObject: an AABB integrated by a midpoint scheme, which also
grows the swept collision hulls the solver in Collision.cpp reads. Keeping
the sweep here means the solver never has to know how a body moved.

===========================================================================
*/
#include "scene/Entity.hpp"

namespace otacon {

/*
==================
computeVelocity

flixel's FlxU.computeVelocity. Acceleration wins over drag - a body being
driven is never also being slowed - and a max of 10000 is flixel's sentinel
for 'unbounded' rather than a real limit.
==================
*/
Real computeVelocity(Real velocity, Real acceleration, Real drag, Real max, Real dt) {
    if (acceleration != R(0)) {
        velocity += acceleration * dt;
    } else if (drag != R(0)) {
        Real d = drag * dt;
        if (velocity - d > R(0))      velocity -= d;
        else if (velocity + d < R(0)) velocity += d;
        else                          velocity = R(0);
    }
    // max == 10000 is flixel's "unbounded" sentinel.
    if (velocity != R(0) && max != R(10000)) {
        if (velocity > max)       velocity = max;
        else if (velocity < -max) velocity = -max;
    }
    return velocity;
}

/*
====================
Entity::refreshHulls

Reset both swept hulls to the body's current rect, before motion grows them.
====================
*/
void Entity::refreshHulls() {
    colHullX = Rect(pos.x, pos.y, size.x, size.y);
    colHullY = Rect(pos.x, pos.y, size.x, size.y);
}

/*
====================
Entity::updateMotion

FlxObject.updateMotion. The midpoint integrator applies half the velocity
change, moves by the resulting averaged velocity, then applies the other
half; that is what makes the motion independent of frame rate to second
order instead of first.

The hulls are then grown by the distance travelled, so a body moving fast
enough to pass through a wall in one step still overlaps it in the hull
the solver tests. That is the whole anti-tunnelling story.
====================
*/
void Entity::updateMotion(Real dt) {
    if (!moves) return;
    if (solid) refreshHulls();
    onFloor = false;

    angle += angularVelocity * dt;   // spin (particles)

    Real vc;
    vc = (computeVelocity(velocity.x, acceleration.x, drag.x, maxVelocity.x, dt) - velocity.x) * R(0.5f);
    velocity.x += vc;
    Real xd = velocity.x * dt;
    velocity.x += vc;

    vc = (computeVelocity(velocity.y, acceleration.y, drag.y, maxVelocity.y, dt) - velocity.y) * R(0.5f);
    velocity.y += vc;
    Real yd = velocity.y * dt;
    velocity.y += vc;

    pos.x += xd;
    pos.y += yd;

    if (!solid) return;
    colVector = {xd, yd};
    colHullX.w += sabs(xd);
    if (xd < R(0)) colHullX.x += xd;
    colHullY.x = pos.x;
    colHullY.h += sabs(yd);
    if (yd < R(0)) colHullY.y += yd;
}

} // namespace otacon
