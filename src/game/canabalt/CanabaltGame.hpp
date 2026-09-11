// CanabaltGame.hpp — the Canabalt game as an Otacon IGame. Owns the Scene,
// the current demo Mode, and the shared GameRandom; routes mode selection, the
// debug grab/drag, and the RNG/quake toggles (F1-F4), and draws the HUD legend.
#pragma once
#include "IGame.hpp"
#include "scene/Scene.hpp"
#include "canabalt/Mode.hpp"
#include "canabalt/PlayerSprite.hpp"
#include "canabalt/GameRandom.hpp"
#include <memory>

namespace canabalt {

class CanabaltGame final : public otacon::IGame {
public:
    void init(otacon::GameContext& ctx) override;
    void handleInput(const otacon::InputFrame& in) override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) override;
    void shutdown() override;                      // free textures before renderer dies

    otacon::Color clearColor() const override { return otacon::Color::rgb(0xb0b0bf); }
    const char* title() const override { return "Canabalt (Otacon)"; }
    const char* statusLine() const override;

private:
    void setMode(int index);
    void applyQuake(bool retrigger);           // push config to the camera
    void updateDrag();                         // debug grab/drag (right mouse)
    otacon::Entity* pick(float lx, float ly) const;
    bool inScene(const otacon::Entity* e) const;
    void drawLegend(otacon::IRenderer& r) const;
    otacon::Entity* entityAt(float lx, float ly) const;            // topmost under cursor
    void drawInspector(otacon::IRenderer& r) const;                // hover entity panel

    otacon::Scene scene_;
    std::unique_ptr<Mode> mode_;
    GameRandom  rng_;                          // shared, switchable (F1)
    otacon::IRenderer* renderer_ = nullptr;    // for modes that create textures
    otacon::IAudio* audio_ = nullptr;          // sound output (from the engine)
    const char* assetDir_ = "";
    int  modeIndex_ = 0;
    const otacon::InputFrame* in_ = nullptr;
    otacon::Entity* grabbed_ = nullptr;

    // startup-quake controls (F2 on/off, F3/F4 intensity)
    bool  quakeEnabled_ = true;
    float quakeIntensity_ = 0.0065f;           // original FlxQuake startup value

    // global particles on/off (F7), independent of the mode
    bool  particlesEnabled_ = true;

    // progressive geometry->sprite reveal (F6 cycles, F8 all on/off)
    SpriteReveal reveal_;
    PlayerSpriteNode playerSprite_;            // F6 sprite path (a scene node)

    // Game-flow chrome drawn at the game level: the PAUSED overlay (shown while
    // the sim is frozen) and a software mouse cursor.
    otacon::TextureHandle pausedTex_ = 0, cursorTex_ = 0;
    int pausedW_ = 0, pausedH_ = 0, cursorW_ = 0, cursorH_ = 0;
    bool cursorVisible_ = true;                 // M toggles the software cursor
    bool soundOn_ = true;                       // F9 mutes/unmutes all audio

    int logicalW_ = 480, logicalH_ = 320;
    mutable char status_[256] = {0};
};

} // namespace canabalt
