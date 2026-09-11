// WorldScene.hpp — Build 6: the real world, and a distance meter.
//
// Everything from Build 5, now with the actual day/night background behind the
// pipes and the scrolling base/ground in front of them. Flappy picks day or
// night at random each run, so we do too. This build also starts *measuring
// distance* — the precursor to scoring — accumulating how far the world has
// scrolled and showing it at the top. Still no collision.
#pragma once
#include "flappy/scene/FlapScene.hpp"
#include <random>

namespace otacon { struct GameContext; }

namespace flappy {

// Not 'final': Build 7 (CollisionScene) extends this, adding collision + skins.
class WorldScene : public FlapScene {
public:
    ~WorldScene() override;
    void init(otacon::GameContext& ctx) override;
    void enter() override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r) const override;
    const char* name() const override { return "6 - background + distance"; }
    const char* status() const override;

protected:
    void drawBackground(otacon::IRenderer& r) const override;  // day or night image
    void drawForeground(otacon::IRenderer& r) const override;  // scrolling ground

private:
    void drawDistance(otacon::IRenderer& r) const;             // the measurement readout

    otacon::IRenderer*    renderer_ = nullptr;
    otacon::TextureHandle dayTex_ = 0, nightTex_ = 0, baseTex_ = 0;
    float        distance_ = 0.f;          // logical px the world has scrolled
    bool         night_    = false;
    std::mt19937 bgRng_{0xB9D1u};          // NOT reseeded per run, so day/night varies across resets
    mutable char status_[64]{};
};

} // namespace flappy
