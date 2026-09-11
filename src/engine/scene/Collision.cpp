/*
===========================================================================

OTACON ENGINE
scene/Collision.cpp - AABB collision separation

A port of flixel's FlxU solveXCollision / solveYCollision, specialised to the
single collision offset the original game uses.

The solver never looks at how a body moved - it reads the swept hulls
Entity::updateMotion already grew, and the unswept extents it stashed in the
other axis' hull. Separating one axis at a time is what gives the response
its character: a body that lands stops dead, with no restitution and no
chance of the two axes fighting each other into a jitter.

===========================================================================
*/
#include "scene/Collision.hpp"

namespace otacon {

/*
==================
RE

flixel's rounding epsilon, used to decide whether two edges really overlap
or merely touch.
==================
*/
static Real RE() { return roundingError(); }

/*
==================
solveX

Horizontal separation. o1/o2 are the two bodies' motion this frame; the
p1hn2 chain works out which side the collision is on, since that decides
both which collide flags apply and which way to push.

An overlap larger than 80% of a hull is rejected rather than resolved: at
that point the bodies are almost certainly on opposite sides of each other
and pushing would teleport one through the other.
==================
*/
bool solveX(Entity& A, Entity& B) {
    Real o1 = A.colVector.x, o2 = B.colVector.x;
    if (o1 == o2) return false;
    A.preCollide(B); B.preCollide(A);

    bool s1 = o1 == R(0), n1 = o1 < R(0), p1 = o1 > R(0);
    bool s2 = o2 == R(0), n2 = o2 < R(0), p2 = o2 > R(0);
    bool p1hn2 = (s1 && n2) || (p1 && s2) || (p1 && n2) ||
                 (n1 && n2 && (sabs(o1) < sabs(o2))) ||
                 (p1 && p2 && (sabs(o1) > sabs(o2)));
    if (p1hn2 ? (!A.collideRight || !B.collideLeft)
              : (!A.collideLeft  || !B.collideRight)) return false;

    Rect h1 = A.colHullX, h2 = B.colHullX;
    Real uw1 = A.colHullY.w, uw2 = B.colHullY.w;   // unswept widths

    if (h1.right() - h2.x < RE() || RE() > h2.right() - h1.x ||
        h1.bottom() - h2.y < RE() || RE() > h2.bottom() - h1.y) return false;

    Real r1, r2;
    if (p1hn2) {
        r1 = n1 ? h1.x + uw1 : h1.right();
        r2 = n2 ? h2.x       : h2.right() - uw2;
    } else {
        r1 = n2 ? -h2.x - uw2 : -h2.right();
        r2 = n1 ? -h1.x       : -h1.right() + uw1;
    }
    Real overlap = r1 - r2;
    if (overlap == R(0) ||
        (!A.fixed && sabs(overlap) > h1.w * R(0.8f)) ||
        (!B.fixed && sabs(overlap) > h2.w * R(0.8f))) return false;

    Real sv1 = B.velocity.x, sv2 = A.velocity.x;
    if (!A.fixed && B.fixed)       A.pos.x -= overlap;
    else if (A.fixed && !B.fixed)  B.pos.x += overlap;
    else if (!A.fixed && !B.fixed) { overlap *= R(0.5f); A.pos.x -= overlap; B.pos.x += overlap; sv1 *= R(0.5f); sv2 *= R(0.5f); }

    if (p1hn2) { A.hitRight(B, sv1); B.hitLeft(A, sv2); }
    else       { A.hitLeft(B, sv1); B.hitRight(A, sv2); }

    if (!A.fixed && overlap != R(0)) {
        if (p1hn2) h1.w -= overlap;
        else { h1.x -= overlap; h1.w += overlap; }
        A.colHullY.x -= overlap;
    }
    if (!B.fixed && overlap != R(0)) {
        if (p1hn2) { h2.x += overlap; h2.w -= overlap; }
        else h2.w += overlap;
        B.colHullY.x += overlap;
    }
    A.colHullX = h1; B.colHullX = h2;
    return true;
}

/*
==================
solveY

Vertical separation. The same shape as solveX, but this is the pass that
sets onFloor, so it is what a platformer's grounded test ultimately reads.
==================
*/
bool solveY(Entity& A, Entity& B) {
    Real o1 = A.colVector.y, o2 = B.colVector.y;
    if (o1 == o2) return false;
    A.preCollide(B); B.preCollide(A);

    bool s1 = o1 == R(0), n1 = o1 < R(0), p1 = o1 > R(0);
    bool s2 = o2 == R(0), n2 = o2 < R(0), p2 = o2 > R(0);
    bool p1hn2 = (s1 && n2) || (p1 && s2) || (p1 && n2) ||
                 (n1 && n2 && (sabs(o1) < sabs(o2))) ||
                 (p1 && p2 && (sabs(o1) > sabs(o2)));
    if (p1hn2 ? (!A.collideBottom || !B.collideTop)
              : (!A.collideTop    || !B.collideBottom)) return false;

    Rect h1 = A.colHullY, h2 = B.colHullY;
    Real uh1 = A.colHullX.h, uh2 = B.colHullX.h;   // unswept heights

    if (h1.right() - h2.x < RE() || RE() > h2.right() - h1.x ||
        h1.bottom() - h2.y < RE() || RE() > h2.bottom() - h1.y) return false;

    Real r1, r2;
    if (p1hn2) {
        r1 = n1 ? h1.y + uh1 : h1.bottom();
        r2 = n2 ? h2.y       : h2.bottom() - uh2;
    } else {
        r1 = n2 ? -h2.y - uh2 : -h2.bottom();
        r2 = n1 ? -h1.y       : -h1.bottom() + uh1;
    }
    Real overlap = r1 - r2;
    if (overlap == R(0) ||
        (!A.fixed && sabs(overlap) > h1.h * R(0.8f)) ||
        (!B.fixed && sabs(overlap) > h2.h * R(0.8f))) return false;

    Real sv1 = B.velocity.y, sv2 = A.velocity.y;
    if (!A.fixed && B.fixed)       A.pos.y -= overlap;
    else if (A.fixed && !B.fixed)  B.pos.y += overlap;
    else if (!A.fixed && !B.fixed) { overlap *= R(0.5f); A.pos.y -= overlap; B.pos.y += overlap; sv1 *= R(0.5f); sv2 *= R(0.5f); }

    if (p1hn2) { A.hitBottom(B, sv1); B.hitTop(A, sv2); }
    else       { A.hitTop(B, sv1); B.hitBottom(A, sv2); }

    if (!A.fixed && overlap != R(0)) {
        h1.y -= overlap;
        if (p1hn2 && B.fixed && B.moves) {           // ride a moving platform in x
            Real carry = B.colVector.x;
            A.pos.x += carry; h1.x += carry; A.colHullX.x += carry;
        } else if (!p1hn2) {
            h1.h += overlap;
        }
    }
    if (!B.fixed && overlap != R(0)) {
        if (p1hn2) { h2.y += overlap; h2.h -= overlap; }
        else {
            h2.h += overlap;
            if (A.fixed && A.moves) {
                Real carry = A.colVector.x;
                B.pos.x += carry; h2.x += carry; B.colHullX.x += carry;
            }
        }
    }
    A.colHullY = h1; B.colHullY = h2;
    return true;
}

/*
=============================================================================

                                 BROAD PHASE

=============================================================================
*/

/*
==================
collideWithGroup

FlxU collideObject:withGroup:. A cheap rejection on the swept hulls first,
then X and then Y - never both at once. Running X to completion before
starting Y is what stops a body wedged in a corner from oscillating.
==================
*/
bool collideWithGroup(Entity& obj, const std::vector<Entity*>& group) {
    if (!obj.exists || !obj.solid) return false;
    bool c = false;
    Real l = obj.pos.x, r = obj.right(), t = obj.pos.y, b = obj.bottom();
    for (Entity* g : group) {
        if (g == &obj || !g->exists || !g->solid) continue;
        // Layer filtering before the geometry: rejecting on a mask is two ands,
        // where rejecting on overlap is four compares and a pair of solves.
        if (!obj.canCollideWith(*g)) continue;
        // A trigger is detected, never separated. Treating it as solid here is
        // what turns a pickup into a wall.
        if (obj.trigger || g->trigger) continue;
        if (r < g->pos.x || l > g->right() || b < g->pos.y || t > g->bottom()) continue;
        c |= solveX(obj, *g);
        c |= solveY(obj, *g);
    }
    return c;
}

/*
=============================================================================

                             OVERLAP QUERIES

=============================================================================
*/

/*
==================
overlap

Plain AABB intersection plus the layer filter. Deliberately does NOT consult
`trigger`: that flag decides whether the solver separates a pair, and a query
has no response to suppress.
==================
*/
bool overlap(const Entity& a, const Entity& b) {
    if (!a.exists || !b.exists) return false;
    if (!a.canCollideWith(b)) return false;
    return !(a.right() < b.pos.x || a.pos.x > b.right() ||
             a.bottom() < b.pos.y || a.pos.y > b.bottom());
}

/*
==================
overlapGroup
==================
*/
void overlapGroup(const Entity& obj, const std::vector<Entity*>& group,
                  std::vector<Entity*>& out) {
    out.clear();
    for (Entity* g : group) {
        if (!g || g == &obj) continue;
        if (overlap(obj, *g)) out.push_back(g);
    }
}

/*
==================
queryRect

The entity-less form, for a question that comes from somewhere other than a
body: a blast radius, a selection marquee, a spawn-placement check.
==================
*/
void queryRect(const Rect& area, const std::vector<Entity*>& group,
               std::vector<Entity*>& out, std::uint32_t mask) {
    out.clear();
    for (Entity* g : group) {
        if (!g || !g->exists) continue;
        if ((mask & g->layer) == 0u) continue;
        if (area.right() < g->pos.x || area.x > g->right() ||
            area.bottom() < g->pos.y || area.y > g->bottom()) continue;
        out.push_back(g);
    }
}

} // namespace otacon
