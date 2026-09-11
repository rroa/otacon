#include "scene/Collision.hpp"

namespace otacon {

static Real RE() { return roundingError(); }

// Port of FlxU.solveXCollision specialized to a single collision offset (the
// only case the original game uses). `colHullY.w` holds each body's *unswept*
// width, `colHullX` the swept hull. See docs/engine-notes for the derivation.
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

// Port of FlxU.solveYCollision (single offset). `colHullX.h` is unswept height.
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

bool collideWithGroup(Entity& obj, const std::vector<Entity*>& group) {
    if (!obj.exists || !obj.solid) return false;
    bool c = false;
    Real l = obj.pos.x, r = obj.right(), t = obj.pos.y, b = obj.bottom();
    for (Entity* g : group) {
        if (g == &obj || !g->exists || !g->solid) continue;
        if (r < g->pos.x || l > g->right() || b < g->pos.y || t > g->bottom()) continue;
        c |= solveX(obj, *g);
        c |= solveY(obj, *g);
    }
    return c;
}

} // namespace otacon
