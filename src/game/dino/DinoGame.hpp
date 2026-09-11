#pragma once
#include "core/math/Random.hpp"
#include "IGame.hpp"
#include "asset/Resources.hpp"
#include "asset/SaveData.hpp"
#include "platform/Window.hpp"
#include "render/IRenderer.hpp"
#include "scene/Entity.hpp"
#include <array>
#include <cstddef>
#include <random>
#include <vector>

namespace dino {

enum class DinoMode {
    Geometry = 0,
    Mechanics,
    Gameplay,
    PixelArt,
};

struct SpriteReveal {
    int level = 0;
    static constexpr int kMax = 3;
    bool player() const { return level >= 1; }
    bool world() const { return level >= 2; }
    bool polish() const { return level >= 3; }
    const char* name() const;
};

struct ObstacleSpec {
    const char* name = "";
    float w = 0;
    float h = 0;
    float y = 0;
    float minSpeed = 0;
    float multipleSpeed = 0;
    float minGap = 0;
    int maxGroup = 1;
    bool bird = false;
};

struct Obstacle {
    otacon::Entity box;
    const ObstacleSpec* spec = nullptr;
    int group = 1;
    float flap = 0;
    bool passed = false;
};

struct Cloud {
    float x = 0;
    float y = 0;
    float gap = 0;
};

class DinoGame final : public otacon::IGame {
public:
    void init(otacon::GameContext& ctx) override;
    void handleInput(const otacon::InputFrame& in) override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) override;
    void shutdown() override;

    otacon::Color clearColor() const override;
    const char* title() const override { return "Dino - Otacon engine"; }
    const char* statusLine() const override;

private:
    static constexpr float kScale = 1.f;
    static constexpr float kSourceW = 600.f;
    static constexpr float kSourceH = 150.f;
    static constexpr float kViewW = 600.f;
    static constexpr float kViewH = 150.f;
    static constexpr float kGameX = 0.f;
    static constexpr float kGameY = 0.f;
    static constexpr float kGroundY = kGameY + 127.f * kScale;
    static constexpr float kTrexX = kGameX + 50.f * kScale;
    static constexpr float kTrexW = 44.f * kScale;
    static constexpr float kTrexH = 47.f * kScale;
    static constexpr float kGravity = 0.6f;
    static constexpr float kInitialJumpVelocity = -10.f;
    static constexpr float kSpeedDropCoefficient = 3.f;

    otacon::IRenderer* renderer_ = nullptr;
    const char* assetDir_ = "";
    otacon::Resources* res_ = nullptr;    // the engine's cache; owns the sheet
    otacon::TextureHandle sheet_ = 0;     // the Chrome offline sprite atlas (1x)
    otacon::SaveData save_;               // HI survives the process now
    int logicalW_ = 480;
    int logicalH_ = 320;
    DinoMode mode_ = DinoMode::Geometry;
    SpriteReveal reveal_;
    const otacon::InputFrame* in_ = nullptr;
    mutable char status_[192]{};

    otacon::Entity player_;
    otacon::Entity groundCollider_;
    std::vector<Obstacle> obstacles_;
    std::vector<Cloud> clouds_;
    otacon::Random rng_{0xD1A0};

    float currentSpeed_ = 6.f;
    float distance_ = 0;
    float score_ = 0;
    int   highScore_ = 0;      // session best, shown as "HI" like Chrome
    float horizonOffset_ = 0;
    float jumpVelocity_ = 0;
    float minJumpY_ = 0;
    float animTimer_ = 0;
    float spawnCooldown_ = 0;
    float restartTimer_ = 0;
    int runFrame_ = 0;
    int seed_ = 0;
    bool jumping_ = false;
    bool reachedMinHeight_ = false;
    bool speedDrop_ = false;
    bool crashed_ = false;
    bool deterministic_ = true;
    bool night_ = false;
    bool cursorVisible_ = false;
    bool showCollisionParts_ = false;

    void setMode(int index);
    int modeIndex() const;
    bool hasMechanics() const;
    bool hasObstacles() const;
    bool wantsPixelArt() const;
    void resetRun();
    void startJump();
    void endJump();
    void updatePlayer(float dtMs);
    void updateWorld(float dtMs);
    void maybeSpawnObstacle();
    void spawnObstacle(const ObstacleSpec& spec);
    void updateEntitiesForDebug();
    bool playerHits(const Obstacle& obstacle) const;
    float randRange(float lo, float hi);
    int randInt(int lo, int hi);

    // Blit a sub-rect of the sprite atlas (sheet pixels) at a destination point.
    void blit(otacon::IRenderer& r, float dx, float dy, float sx, float sy, float sw, float sh) const;
    void drawScore(otacon::IRenderer& r) const;

    void drawWorld(otacon::IRenderer& r) const;
    void drawGround(otacon::IRenderer& r, bool pixel) const;
    void drawCloud(otacon::IRenderer& r, const Cloud& cloud, bool pixel) const;
    void drawPlayerGeometry(otacon::IRenderer& r) const;
    void drawPlayerPixel(otacon::IRenderer& r) const;
    void drawObstacle(otacon::IRenderer& r, const Obstacle& obstacle, bool pixel) const;
    void drawCactus(otacon::IRenderer& r, float x, float y, float w, float h, int group, bool large, bool pixel) const;
    void drawBird(otacon::IRenderer& r, float x, float y, float w, float h, int frame, bool pixel) const;
    void drawHud(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) const;
    void drawColliderParts(otacon::IRenderer& r) const;
};

} // namespace dino
