#include "canabalt/Sequence.hpp"
#include "canabalt/Player.hpp"
#include <algorithm>
#include <cmath>

using namespace otacon;

namespace canabalt {
namespace {

// --- world units -----------------------------------------------------------
constexpr float kTileSize = 16.f;   // collision/grid tile
constexpr float kDecSize  = 20.f;   // decoration grid (used in the gap formula)
constexpr float kScreenW  = 480.f;
constexpr float kScreenH  = 480.f;  // the design uses 480 for vertical extents too

// --- gap sizing (distance between buildings), worked in tiles --------------
// CLASS NOTE: the gap scales with the player's speed — the faster you run, the
// wider the jumps the generator dares to ask of you. This single coupling is
// what makes Canabalt's difficulty curve self-balancing.
constexpr float kGapSpeedScale = 0.75f;   // gap derives from 75% of current speed…
constexpr float kGapTileScale  = 0.75f;   // …then scaled again into tile units
constexpr float kMinGapRatio   = 0.40f;   // min gap = 40% of the max gap…
constexpr float kMinGapFloor   = 4.f;     // …but never under 4 tiles

// --- building width (tiles) ------------------------------------------------
constexpr float kMinWidthSlow  = 15.f;    // wide, forgiving floor while you're slow
constexpr float kMinWidthFast  = 6.f;     // narrow floors allowed once you're fast
constexpr float kSlowSpeedRatio = 0.8f;   // "slow" == below 80% of max speed
constexpr float kWidthRangeMult = 2.f;    // actual width varies up to minW * this

// --- vertical change between consecutive roofs (tiles) ---------------------
constexpr float kJumpMarginTiles = 2.f;   // headroom kept under the theoretical max jump
constexpr float kJumpTilesAtMax  = 6.f;   // tiles clearable at a full jump-limit
constexpr float kJumpLimitMax    = 0.35f; // Player.m jump-limit cap (seconds)
constexpr float kDropMarginTiles = 4.f;   // keep some building below a drop
constexpr float kMaxDropTiles    = 10.f;  // never drop more than this

// --- sanity thresholds that downgrade an unfair special type back to ROOF --
constexpr float kCollapseMaxSlope = 0.015f;
constexpr float kBombMinWidthMul  = 1.5f;
constexpr float kCraneMinHeight   = kTileSize * 2;
constexpr float kCraneMinWidth    = kTileSize * 24;
constexpr float kLegMaxWidth      = kTileSize * 28;

// --- special-type block dimensions -----------------------------------------
constexpr float kLegBlockWidth = 120.f;
constexpr float kBlockPadX     = 10.f;    // blocks overhang +10px (Sequence.m)
constexpr unsigned kBuildingColor = 0x2b2b33;

} // namespace

void SequenceField::setBlock(Entity& b, float x, float y, float w, float h) {
    b.pos = {R(x), R(y)};
    b.size = {R(w), R(h)};
    b.fixed = true; b.moves = true; b.solid = true;       // static, but hulls refresh
    b.visible = true; b.renderable = true;
    b.velocity = {R(0), R(0)}; b.acceleration = {R(0), R(0)};
    b.color = Color::rgb(kBuildingColor);
}

void SequenceField::init(Player* player, GameRandom& rng) {
    player_ = player;
    rng_ = &rng;
    rng_->restart();                          // reseed for a fresh (or reproducible) city
    curIndex_  = 0;
    nextIndex_ = 3 + int(rng_->unit() * 3);   // PlayState: random*3 + 3
    nextType_  = 1;
    lastType_ = thisType_ = 0;
    reset(seqA_, seqB_);                       // curIndex 0 -> launch hallway
    reset(seqB_, seqA_);                       // curIndex 1 -> second building
    roofY0_ = seqA_.y;
}

// Faithful port of Sequence.m -reset (the geometry/collision half).
void SequenceField::reset(Seq& self, const Seq& seq) {
    self.ceilingActive = false;
    self.passed = false;
    self.wallType   = int(rng_->unit() * 4);   // 1 of 4 wall styles
    self.windowType = int(rng_->unit() * 4);
    self.hasEscape  = rng_->unit() < 0.5f;      // fire escape on half the buildings
    self.decorSeed  = std::uint32_t(rng_->unit() * 4.2e9f) | 1u;  // stable per-building randomness
    self.billH      = 0.f;
    self.hallPixels = 0.f;
    self.legY = -480.f; self.legLanded = false;
    self.collapsing = false; self.sinkVY = 0.f; self.sinkBlock = false;
    self.bombY = -80.f; self.bombLanded = false;
    const float vx    = toFloat(player_->velocity.x);
    const float maxVx = toFloat(player_->maxVelocity.x);

    // Every few buildings, inject one special archetype.
    int type = ROOF;
    static const int kTypes[6] = {HALLWAY, COLLAPSE, BOMB, CRANE, BILLBOARD, LEG};
    if (curIndex_ == nextIndex_) {
        type = kTypes[nextType_];
        nextIndex_ += 3 + int(rng_->unit() * 5);
        nextType_   = int(rng_->unit() * 6);
    }

    // The first two buildings are fixed "launch" geometry.
    self.launch = (curIndex_ == 0);
    if (curIndex_ == 0) {
        self.x = -4 * kTileSize; self.y = 5 * kTileSize;
        self.width = 60 * kTileSize; self.height = kScreenH - 5 * kTileSize;
        type = HALLWAY;
    } else if (curIndex_ == 1) {
        self.x = seq.x + seq.width + 10 * kTileSize; self.y = 15 * kTileSize;
        self.width = 42 * kTileSize; self.height = kScreenH - 15 * kTileSize;
    }

    // Taller hallways at higher speed (gives you room to clear it).
    int hallHeight = 0;
    if (type == HALLWAY) {
        if      (vx > 640) hallHeight = 7;
        else if (vx > 480) hallHeight = 6;
        else if (vx > 320) hallHeight = 5;
        else if (curIndex_ > 0) hallHeight = 4;
        else               hallHeight = 3;
    }

    // ---- the heart of the generator: gap, width and vertical step ----------
    const float screenTiles = std::ceil(kScreenW / kTileSize) + 2;                 // 32
    const float fg = rng_->unit();                                                 // shared roll

    const float maxGap = (vx * kGapSpeedScale / kDecSize) * kGapTileScale;
    const float minGap = std::max(kMinGapFloor, maxGap * kMinGapRatio);
    const float gap    = float(int(minGap + fg * (maxGap - minGap)));              // tiles

    float minW = screenTiles - gap;
    if (minW < kMinWidthSlow && vx < maxVx * kSlowSpeedRatio) minW = kMinWidthSlow;
    else minW = std::max(minW, kMinWidthFast);
    const float maxW = minW * kWidthRangeMult;

    const float reachableJump = kJumpTilesAtMax * toFloat(player_->jumpLimit()) / kJumpLimitMax;
    float maxJump = std::min(seq.y / kTileSize - kJumpMarginTiles - hallHeight, reachableJump);
    if (maxJump > 0) maxJump = std::ceil(maxJump * (1 - fg));
    const float maxDrop = std::min(kMaxDropTiles, seq.height / kTileSize - kDropMarginTiles);

    if (curIndex_ > 1) {
        self.x = seq.x + seq.width + gap * kTileSize;
        // drop<0 steps the roof UP (toward the player), drop>0 steps it down.
        float drop = float(int(rng_->unit() * maxDrop - maxJump));
        if (type == HALLWAY && gap > 10) drop = 0;
        if (drop == 0) drop -= 1;                          // never perfectly flat
        self.y = seq.y + drop * kTileSize;
        self.height = kScreenH - self.y;
        self.width = std::floor(minW + rng_->unit() * maxW) * kTileSize;
    }

    // If a special type would be unfair/silly at this size, fall back to ROOF.
    if ((type == COLLAPSE && self.width / self.height > vx * kCollapseMaxSlope) ||
        (type == BOMB     && self.width < vx * kBombMinWidthMul) ||
        (type == CRANE    && (self.height < kCraneMinHeight || self.width < kCraneMinWidth)) ||
        (type == LEG      && self.width > kLegMaxWidth)) {
        type = ROOF;
        nextIndex_ = curIndex_ + 1;
    }

    // ---- build the main collision block, shaped per type -------------------
    if (type == LEG) {
        // A narrow obstacle the player must clear, centered on the building; the
        // giant leg drops out of the sky onto the previous roof (seq.y).
        setBlock(self.block, self.x + (self.width - kLegBlockWidth) / 2.f,
                 seq.y + 3, kLegBlockWidth + 20, kScreenH);
        self.legLandY = seq.y;
    } else if (type == CRANE || type == BILLBOARD) {
        self.width = std::ceil(self.width / (kTileSize * 2)) * kTileSize * 2;
        if (type == BILLBOARD) {
            int bh = (3 + int(rng_->unit() * 5)) * int(kTileSize * 2);
            self.billH = float(bh);
            if (self.width > bh * 2) {
                int diff = int(self.width) - bh * 2;
                self.x += (diff > 4 * kTileSize) ? 4 * kTileSize : (self.width - bh * 2);
                self.width = float(bh * 2);
            }
        }
        // Crane/billboard collision is just a thin ledge at roof height.
        setBlock(self.block, self.x, self.y, self.width + kBlockPadX, 2 * kTileSize);
    } else {
        setBlock(self.block, self.x, self.y, self.width + kBlockPadX, self.height + 2 * kTileSize);
    }
    self.type = type;

    // Hallway: a ceiling block that forces you to stay low until you exit, plus
    // windows at each end the player smashes through (resets the broken flags).
    self.brokeL = self.brokeR = false;
    if (type == HALLWAY) {
        hallHeight *= int(kTileSize);
        self.hallPixels = float(hallHeight);
        self.ceilingActive = true;
        setBlock(self.ceiling, self.x, -128, self.width, self.y - hallHeight + 128);
        self.winL = self.x + kTileSize;
        self.winR = self.x + self.width - kTileSize;
    } else {
        self.winL = self.winR = 1e9f;
    }

    // The building renders at its full (pre-shrink) rect — the shrink below only
    // adjusts y/height as an estimate for the *next* building's placement, so the
    // visual must keep the dims the collision block was built with.
    self.drawY = self.y; self.drawH = self.height;

    // Collapse: estimate the building shrinking as it falls.
    if (type == COLLAPSE) {
        int cd = std::min(int((self.width / kTileSize) * 0.5f), int(self.height / kTileSize - 1));
        self.height -= cd * kTileSize;
        self.y += cd * kTileSize;
    }
    if (type == BOMB) self.bombLandY = self.y - 30.f;   // bomb rests just above the roof

    // Small roof obstacles (Obstacle.m): hurdles you stumble over. Roofs get a
    // few at low odds; hallways get more, and the launch hallway gets two fixed.
    self.obCount = 0;
    auto addOb = [&](float ox, float oy, bool alt) {
        if (self.obCount >= Seq::kMaxOb) return;
        Seq::Ob& o = self.obs[self.obCount++];
        o.x = ox - 9.f; o.y = oy - 18.f;        // 18x18 sprite sitting on the roof
        o.alt = alt; o.frame = int(rng_->unit() * (alt ? 2 : 4));
        o.active = true; o.knocked = false;
        o.vx = o.vy = o.angle = o.angVel = 0.f;
    };
    if (type == ROOF && curIndex_ > 1) {
        for (int i = 0; i < 3; ++i)
            if (rng_->unit() < 0.15f) addOb(self.x + self.width / 8.f + rng_->unit() * (self.width / 2.f), self.y, true);
    } else if (type == HALLWAY) {
        if (curIndex_ == 0) { addOb(32 * kTileSize, self.y, false); addOb(48 * kTileSize, self.y, false); }
        else for (int i = 0; i < 3; ++i)
            if (rng_->unit() < 0.65f) addOb(self.x + self.width / 8.f + rng_->unit() * (self.width / 2.f), self.y, false);
    }

    // Pigeons: a flock that sits on non-hallway roofs and flushes as you pass.
    self.doveCount = 0;
    if (type != HALLWAY && rng_->unit() < 0.35f) {
        int n = std::min(int(Seq::kMaxDove), 1 + int(self.width / 120.f * 2.f));
        for (int i = 0; i < n; ++i) {
            Seq::Dv& d = self.doves[self.doveCount++];
            d.x = self.x + rng_->unit() * (self.width - 10.f);
            d.y = self.y - 10.f;
            d.facing = rng_->unit() > 0.5f;
            d.trigger = self.x + rng_->unit() * (d.x - self.x) * 0.5f;
            d.active = true; d.flying = false;
            d.vx = d.vy = d.ax = d.ay = d.anim = 0.f;
            d.r1 = rng_->unit(); d.r2 = rng_->unit(); d.r3 = rng_->unit();
            if (self.doveCount >= Seq::kMaxDove) break;
        }
    }

    ++curIndex_;
}

void SequenceField::update(Real dt, const Camera& cam) {
    smashes_.clear();
    stomps_.clear();
    collapses_.clear();
    bombs_.clear();
    bombDrops_.clear();
    legDrops_.clear();
    obHits_.clear();
    flushes_.clear();
    const float px = toFloat(player_->pos.x);
    const float pw = toFloat(player_->size.x);
    const float py = toFloat(player_->pos.y);
    const float ph = toFloat(player_->size.y);
    const float fdt = toFloat(dt);
    // Drop any giant leg once the player gets within ~480px of it; on landing it
    // stomps (gibs + quake) and the building below it crumbles (facade sinks).
    auto dropLeg = [&](Seq& s) {
        if (s.type != LEG || s.legLanded) return;
        float originX = s.x + s.width * 0.5f;
        if (px > originX - 480.f) {
            if (s.legY <= -480.f) legDrops_.push_back(originX);   // first frame of the drop
            s.legY += 1600.f * fdt;
            if (s.legY >= s.legLandY + 2.f) {
                s.legY = s.legLandY + 2.f; s.legLanded = true;
                stomps_.push_back(originX);
                s.collapsing = true; s.sinkVY = 60.f; s.sinkBlock = false;  // visual collapse
            }
        }
    };
    dropLeg(seqA_); dropLeg(seqB_);

    // COLLAPSE buildings sink when stepped on; BOMBs drop and become lethal.
    auto dynamics = [&](Seq& s) {
        if (s.type == COLLAPSE && !s.collapsing && px + pw >= s.x && px < s.x + s.width) {
            s.collapsing = true; s.sinkVY = 60.f; s.sinkBlock = true;
            collapses_.push_back(s.x + s.width * 0.5f);
        }
        if (s.collapsing) {
            s.sinkVY = std::min(300.f, s.sinkVY + 40.f * fdt);
            float dy = s.sinkVY * fdt;
            s.drawY += dy;                                 // facade sinks
            if (s.sinkBlock) {
                // Carry a rider down with the block. Without this the floor drops
                // out from under the player's feet, they float a hair above it,
                // never register as grounded, and so can never jump off — the one
                // thing a collapse is supposed to make you do. Glue them to the
                // sinking roof so the leap stays in their hands.
                //
                // We can't test player_->onFloor here: updateMotion() already
                // cleared it this frame (it's re-set later, by the collision pass
                // that runs after us). So detect riding geometrically — feet level
                // with the block top and not moving upward (i.e. not mid-jump).
                const float top  = toFloat(s.block.pos.y);
                const float feet = py + ph;
                const bool riding = toFloat(player_->velocity.y) >= 0.f
                                    && px + pw > s.x && px < s.x + s.width
                                    && feet > top - 8.f && feet < top + 8.f;
                s.block.pos.y = s.block.pos.y + R(dy);     // collision sinks too
                if (riding) player_->pos.y = player_->pos.y + R(dy);
            }
        }
        if (s.type == BOMB) {
            float bx = s.x + s.width * 0.5f;
            if (!s.bombLanded && px > bx - 480.f) {
                if (s.bombY <= -80.f) bombDrops_.push_back(bx);   // first frame of the drop
                s.bombY += 1200.f * fdt;
                if (s.bombY >= s.bombLandY) { s.bombY = s.bombLandY; s.bombLanded = true; bombs_.push_back(bx); }
            }
            if (s.bombLanded && !player_->dead_) {         // touching a landed bomb is fatal
                float bl = bx - 20.f, bt = s.bombY;
                if (px + pw > bl && px < bl + 40.f && py + ph > bt && py < bt + 60.f) {
                    player_->dead_ = true; player_->epitaph = "bomb";
                }
            }
        }
    };
    dynamics(seqA_); dynamics(seqB_);

    // Roof obstacles: stumble the player on contact, then tumble away.
    auto obstacles = [&](Seq& s) {
        for (int i = 0; i < s.obCount; ++i) {
            Seq::Ob& o = s.obs[i];
            if (!o.active) continue;
            if (!o.knocked) {
                if (px + pw > o.x && px < o.x + 18.f && py + ph > o.y && py < o.y + 18.f) {
                    player_->velocity.x = player_->velocity.x * R(0.7f);   // stumble: lose speed
                    player_->stumble = true;
                    o.knocked = true;
                    obHits_.push_back(o.x);
                    o.vx = toFloat(player_->velocity.x);
                    o.vy = -120.f; o.angVel = 400.f;
                }
            } else {
                o.vy += 320.f * fdt;
                o.x += o.vx * fdt; o.y += o.vy * fdt;
                o.angle += o.angVel * fdt;
                if (o.y > 500.f) o.active = false;
            }
        }
    };
    obstacles(seqA_); obstacles(seqB_);

    // Pigeons flush (fly up and away) once the player crosses their trigger.
    const float pvx = toFloat(player_->velocity.x);
    auto doveStep = [&](Seq& s) {
        for (int i = 0; i < s.doveCount; ++i) {
            Seq::Dv& d = s.doves[i];
            if (!d.active) continue;
            d.anim += fdt;
            if (!d.flying && px > d.trigger) {
                d.flying = true;
                flushes_.push_back(d.x);
                d.vy = -50.f - d.r1 * 50.f;                       // initial upward kick
                d.ay = -50.f - d.r2 * 300.f;                      // keeps accelerating up
                float v = (pvx - 300.f) * d.r3;
                d.ax = d.facing ? v : -v;
                d.anim = 0.f;
            }
            if (d.flying) {
                d.vy += d.ay * fdt; d.vx += d.ax * fdt;
                d.x += d.vx * fdt;  d.y += d.vy * fdt;
                if (d.y < -200.f) d.active = false;
            }
        }
    };
    doveStep(seqA_); doveStep(seqB_);
    // Reset a sequence once it has scrolled fully off the left edge — this is
    // the leapfrog that makes the level infinite with only two buildings live.
    auto step = [&](Seq& self, const Seq& other) {
        float screenX = std::floor(self.x) + std::floor(toFloat(cam.scroll.x));   // scrollFactor 1
        if (screenX + self.width < 0) reset(self, other);
        if (!self.passed && px > self.x) {
            self.passed = true; lastType_ = thisType_; thisType_ = self.type;
        }
        // Smash each hallway window once as the player runs through it.
        if (!self.brokeL && px > self.winL) { smashes_.push_back(self.winL); self.brokeL = true; }
        if (!self.brokeR && px > self.winR) { smashes_.push_back(self.winR); self.brokeR = true; }
    };
    step(seqA_, seqB_);
    step(seqB_, seqA_);

    // Refresh collision hulls (static blocks still need updateMotion) + rebuild
    // the list the player collides against.
    collide_.clear();
    for (Seq* s : {&seqA_, &seqB_}) {
        s->block.update(dt);
        collide_.push_back(&s->block);
        if (s->ceilingActive) { s->ceiling.update(dt); collide_.push_back(&s->ceiling); }
    }
}

bool SequenceField::onCrane() const {
    const float px = toFloat(player_->pos.x);
    for (const Seq* s : {&seqA_, &seqB_})
        if (s->type == CRANE && px >= s->x && px < s->x + s->width) return true;
    return false;
}

void SequenceField::appendObstacles(std::vector<ObView>& out) {
    for (Seq* s : {&seqA_, &seqB_})
        for (int i = 0; i < s->obCount; ++i) {
            const Seq::Ob& o = s->obs[i];
            if (o.active) out.push_back({o.x, o.y, o.angle, o.frame, o.alt, o.knocked});
        }
}

void SequenceField::appendDoves(std::vector<DoveView>& out) {
    for (Seq* s : {&seqA_, &seqB_})
        for (int i = 0; i < s->doveCount; ++i) {
            const Seq::Dv& d = s->doves[i];
            if (d.active) out.push_back({d.x, d.y, d.anim, d.facing, d.flying});
        }
}

void SequenceField::appendRenderBlocks(std::vector<Entity*>& out) {
    for (Seq* s : {&seqA_, &seqB_}) {
        out.push_back(&s->block);
        if (s->ceilingActive) out.push_back(&s->ceiling);
    }
}

void SequenceField::appendPieces(std::vector<Piece>& out) {
    for (Seq* s : {&seqA_, &seqB_}) {
        if (s->type == HALLWAY) {
            // One piece carries the whole tunnel (opening height in aux + smash state).
            out.push_back({HALLWAY, s->x, s->drawY, s->width, s->drawH, s->wallType, s->windowType,
                           false, s->hallPixels, s->decorSeed, s->brokeL, s->brokeR, 0.f, 0.f, s->launch});
        } else {
            out.push_back({s->type, s->x, s->drawY, s->width, s->drawH, s->wallType, s->windowType,
                           s->hasEscape, s->billH, s->decorSeed, false, false, s->legY, s->bombY, false});
        }
    }
}

} // namespace canabalt
