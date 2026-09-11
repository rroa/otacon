// Hud.hpp — the game-flow UI: the live distance counter and the game-over panel.
//
// These are screen-space nodes (no camera scroll): they draw at fixed logical
// pixels so they stay pinned while the city races behind them. HudNode renders
// the metres-run odometer from the hud.png glyph strip (digits 0-9 + an "m",
// each its own width, packed left to right). GameOverNode is the death overlay —
// the dark bands, the GAME OVER graphic, the epitaph and the retry prompt.
#pragma once
#include "asset/Resources.hpp"
#include "render/Atlas.hpp"
#include "scene/Node.hpp"
#include "render/IRenderer.hpp"

namespace canabalt {

// Top-right metres-run counter, drawn from the hud.png glyph strip.
class HudNode final : public otacon::Node {
public:
    void load(otacon::Resources* res, const char* assetDir);   // images/hud.png
    // The odometer strip, described once as named regions rather than divided
    // by hand at every draw site.
    otacon::Atlas atlas_;
    void destroy(otacon::IRenderer* r);
    void setDistance(int metres) { distance_ = metres < 0 ? 0 : metres; }

    void render(otacon::IRenderer& r, const otacon::Camera& cam) const override;

private:
    void glyph(otacon::IRenderer& r, int index, float x, float y) const;  // 0-9, 10='m'

    otacon::TextureHandle tex_ = 0;
    int   texW_ = 0, texH_ = 0;
    int   distance_ = 0;
};

// Full-screen title screen: the city-skyline backdrop, the CANABALT wordmark and
// a "press jump" prompt. Shown until the first run begins.
class TitleNode final : public otacon::Node {
public:
    void load(otacon::Resources* res, const char* assetDir);   // title.png + title2.png
    void destroy(otacon::IRenderer* r);
    void render(otacon::IRenderer& r, const otacon::Camera& cam) const override;

private:
    otacon::TextureHandle bgTex_ = 0, logoTex_ = 0;
    int   logoW_ = 0, logoH_ = 0;
};

// Full-screen death overlay: dark bands + GAME OVER graphic + epitaph + prompt.
class GameOverNode final : public otacon::Node {
public:
    void load(otacon::Resources* res, const char* assetDir);   // gameover.png + exit
    void destroy(otacon::IRenderer* r);

    void show(int distance, const char* cause, bool newRecord);   // arm on death
    void hide() { active_ = false; }
    bool active() const { return active_; }

    // The EXIT button lives at the top-right. Track the cursor (for hover art) and
    // hit-test it (so a click there leaves instead of retrying).
    void setMouse(float x, float y) { mouseX_ = x; mouseY_ = y; }
    bool exitHit(float x, float y) const {
        return x >= 480.f - float(exitW_) && x <= 480.f && y >= 0.f && y <= float(exitH_);
    }

    void render(otacon::IRenderer& r, const otacon::Camera& cam) const override;

private:
    otacon::TextureHandle goTex_ = 0, exitTex_ = 0, exitOnTex_ = 0, recordTex_ = 0;
    int   goW_ = 0, goH_ = 0, exitW_ = 0, exitH_ = 0, recordW_ = 0, recordH_ = 0;
    bool  active_ = false;
    bool  newRecord_ = false;
    int   distance_ = 0;
    float mouseX_ = -1, mouseY_ = -1;
    char  cause_[48] = {0};
};

} // namespace canabalt
