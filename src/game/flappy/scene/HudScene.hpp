// HudScene.hpp — Build 9: the finished game.
//
// Build 8 was playable; this dresses it for release. The score uses the real
// number sprites, the run is framed by a proper menu (a "get ready" splash you
// tap to start, a game-over splash you tap to retry), and the game finally makes
// noise — wing on flap, point on score, hit + die on a crash. The play/physics
// are entirely inherited from ScoreScene; this build adds presentation + a small
// Ready / Playing / GameOver state machine on top.
#pragma once
#include "flappy/scene/ScoreScene.hpp"
#include "audio/IAudio.hpp"     // SoundId
#include <string>

namespace otacon { struct GameContext; struct InputFrame; class IAudio; }

namespace flappy {

class HudScene final : public ScoreScene {
public:
    ~HudScene() override;
    void init(otacon::GameContext& ctx) override;
    void enter() override;
    void handleInput(const otacon::InputFrame& in) override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r) const override;
    const char* name() const override { return "9 - HUD + menu + sound"; }
    const char* status() const override;
    void        setDemo(bool d) override { demo_ = d; }

private:
    enum class Phase { Ready, Playing, GameOver };

    void flap();                                       // apply the upward kick + wing sound
    void demoControl(otacon::Real dt);                 // self-playing autopilot
    int  medalTier() const;                            // 0 none, 1 bronze .. 4 platinum
    void loadBest();                                   // read persisted high score
    void saveBest() const;                             // write it back
    void drawNumber(otacon::IRenderer& r, int value, float centerX, float top, float scale) const;
    void drawNumberRight(otacon::IRenderer& r, int value, float rightX, float top, float scale) const;
    void drawMessage(otacon::IRenderer& r) const;
    void drawGameOverPanel(otacon::IRenderer& r) const;
    static void drawDisc(otacon::IRenderer& r, float cx, float cy, float radius, otacon::Color c);

    otacon::IRenderer*    renderer_ = nullptr;
    otacon::IAudio*       audio_    = nullptr;
    otacon::TextureHandle digits_[10] = {};
    otacon::TextureHandle messageTex_ = 0, gameoverTex_ = 0;
    otacon::SoundId       wing_ = 0, point_ = 0, hit_ = 0, die_ = 0, swoosh_ = 0;

    Phase        phase_      = Phase::Ready;
    int          best_       = 0;       // high score, persisted across sessions
    bool         newBest_    = false;   // this run beat the stored best (shows the NEW badge)
    std::string  savePath_;             // where the high score is persisted
    float        readyTime_  = 0.f;     // drives the hover bob on the menu
    bool         diePlayed_  = false;   // the "die" thud fires once, when the body lands
    bool         demo_       = false;   // autopilot for GIF recording
    float        demoGoTime_ = 0.f;     // time spent on the game-over screen in demo mode
    mutable char status_[48]{};
};

} // namespace flappy
