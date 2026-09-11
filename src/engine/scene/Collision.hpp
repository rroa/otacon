/*
===========================================================================

OTACON ENGINE
scene/Collision.hpp - AABB collision separation

===========================================================================
*/
#pragma once
#include "scene/Entity.hpp"
#include <vector>

namespace otacon {

// Resolve one axis between two entities; returns true if they touched.
bool solveX(Entity& a, Entity& b);
bool solveY(Entity& a, Entity& b);

// Collide a single moving entity against a group of (usually fixed) entities.
// Mirrors FlxU collideObject:withGroup: broad-phase reject, then X then Y.
bool collideWithGroup(Entity& obj, const std::vector<Entity*>& group);

} // namespace otacon
