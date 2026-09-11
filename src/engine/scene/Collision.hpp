/*
===========================================================================

OTACON ENGINE
scene/Collision.hpp - AABB collision separation

===========================================================================
*/
#pragma once
#include "core/math/Rect.hpp"
#include "scene/Entity.hpp"
#include <vector>

namespace otacon {

// Resolve one axis between two entities; returns true if they touched.
bool solveX(Entity& a, Entity& b);
bool solveY(Entity& a, Entity& b);

// Collide a single moving entity against a group of (usually fixed) entities.
// Mirrors FlxU collideObject:withGroup: broad-phase reject, then X then Y.
// Respects layer/mask filtering and skips triggers -- see Entity.
bool collideWithGroup(Entity& obj, const std::vector<Entity*>& group);

/*
=============================================================================

                             OVERLAP QUERIES

  Detection without response. A trigger volume, an area-of-effect, a "what is
  standing here" question -- all of these want to know WHAT overlaps, not to be
  pushed apart. Keeping them separate from the solver is what lets a pickup be
  a pickup rather than a wall.

=============================================================================
*/

// Do these two bodies overlap, respecting layer/mask? Ignores `trigger`, since
// that flag is about response and this function has none.
bool overlap(const Entity& a, const Entity& b);

// Everything in `group` overlapping `obj`. `out` is cleared first; `obj` is
// never included.
void overlapGroup(const Entity& obj, const std::vector<Entity*>& group,
                  std::vector<Entity*>& out);

// Everything in `group` overlapping an arbitrary rect -- for a query that has no
// entity behind it, like a blast radius or a selection box.
void queryRect(const Rect& area, const std::vector<Entity*>& group,
               std::vector<Entity*>& out, std::uint32_t mask = 0xFFFFFFFFu);

} // namespace otacon
