// Config.hpp — tunables for the Flappy Bird build, gathered in one place.
//
// Everything the scenes share — canvas size, the bird's pinned position, the
// approximate day-theme palette, and the physics knobs — lives here as constexpr
// so each build reads the same numbers and there are no magic literals scattered
// across the scene classes.
#pragma once
#include "render/RenderTypes.hpp"

namespace flappy::cfg {

// The original Flappy Bird canvas is 288x512. We simulate and draw at that
// logical resolution (so future sprite art lines up 1:1) and scale the OS window
// up by an integer factor for visibility / crisp pixels.
inline constexpr int kLogicalW   = 288;
inline constexpr int kLogicalH   = 512;
inline constexpr int kWindowScale = 2;

// The bird never moves horizontally; only its height changes. X is its left edge.
inline constexpr float kBirdX     = 80.f;
inline constexpr float kBirdSize  = 26.f;                          // square stand-in for the 34x24 sprite
inline constexpr float kBirdStartY = kLogicalH * 0.45f - kBirdSize * 0.5f;
// The real bird sprite's native dimensions (yellowbird-*.png).
inline constexpr float kBirdSpriteW = 34.f;
inline constexpr float kBirdSpriteH = 24.f;

// Build 9 HUD assets: the score number sprites (0-9.png) and the two splash
// images, plus the ready-screen hover.
inline constexpr float kDigitW    = 24.f, kDigitH    = 36.f;
inline constexpr float kMsgW      = 184.f, kMsgH     = 267.f;   // message.png (get ready)
inline constexpr float kGameOverW = 192.f, kGameOverH = 42.f;   // gameover.png
inline constexpr float kBobAmp    = 6.f;     // ready-screen vertical hover amplitude, px
inline constexpr float kBobOmega  = 6.f;     // ready-screen hover speed, rad/s

// Build 5: wing animation + velocity-based tilt.
inline constexpr float kAnimStep = 0.09f;     // seconds per wing frame (up→mid→down→mid)
inline constexpr float kTiltUp   = -20.f;     // degrees (nose up) while rising / just-flapped
inline constexpr float kTiltDown = 90.f;      // degrees (nose straight down) at full dive
inline constexpr float kTiltRate = 0.16f;     // degrees added per (px/s) of downward velocity

// Build 1 only: constant manual rise/sink speed, in logical px per second.
// (Build 1.b replaces this constant velocity with real gravity.)
inline constexpr float kManualSpeed = 150.f;

// Build 1.b: gravity is an acceleration (px/s^2), a flap is an instantaneous
// upward velocity (px/s). Gravity is nudged at runtime to *feel* the parameter.
inline constexpr float kGravity     = 1500.f;
inline constexpr float kFlapImpulse = -450.f;
inline constexpr float kGravityStep = 150.f;     // per key-press nudge
inline constexpr float kGravityMin  = 0.f;
inline constexpr float kGravityMax  = 4000.f;

// Build 3: pipes scroll from the right toward the (horizontally fixed) bird —
// the motion you actually perceive comes from them, not the bird. Geometry only
// here; the pipe texture arrives in Build 4. No collision yet.
inline constexpr float kPipeWidth   = 52.f;     // matches pipe-green.png (52 wide)
inline constexpr float kPipeGap     = 120.f;    // vertical opening the bird flies through
inline constexpr float kPipeSpacing = 170.f;    // horizontal distance between pipe pairs
inline constexpr float kPipeMargin  = 50.f;     // keep the gap away from top/bottom edges
inline constexpr float kScrollSpeed = 130.f;    // world scroll speed, px/second
// Build 4 textures the pipes (pipe-green.png is 52x320). The lip/cap is drawn at
// native size at the gap mouth; only the shaft is stretched to reach the edge, so
// short and tall pipes both look right.
inline constexpr float kPipeSpriteH = 320.f;
inline constexpr float kPipeCapH    = 26.f;

// Build 6: the real background (day/night, chosen at random per run) and the
// scrolling base/ground (base.png is 336x112). The ground line is the top of the
// base; pipes and the bird live above it.
inline constexpr float kBaseW   = 336.f;
inline constexpr float kBaseH   = 112.f;
inline constexpr float kGroundY = kLogicalH - kBaseH;   // 400 — top of the ground

// Palette — eyeballed approximations of the day-theme assets. No PNG is loaded
// yet for these; they stand in until the real sprites arrive in a later build.
inline constexpr otacon::Color kSky {0.306f, 0.753f, 0.792f};      // ~#4EC0CA daylight sky
inline constexpr otacon::Color kBird{0.980f, 0.820f, 0.090f};      // mellow yellow bird body
inline constexpr otacon::Color kPipe{0.455f, 0.749f, 0.180f};      // ~#74BF2E placeholder green

} // namespace flappy::cfg
