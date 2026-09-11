#include "canabalt/Hud.hpp"
#include "asset/Resources.hpp"
#include "asset/Image.hpp"
#include <cstdio>
#include <cstring>
#include <string>

using namespace otacon;

namespace canabalt {

namespace {
constexpr int   kViewW = 480;
constexpr float kHudY = 5.f;             // top margin (PlayState: 2+3)
constexpr float kHudRight = 475.f;       // right edge of the odometer
// Per-glyph widths in hud.png: digits 0-9 then 'm' (index 10). Sum = 134 (texW).
constexpr int kGlyphW[11] = {12, 8, 12, 12, 12, 12, 12, 12, 12, 12, 18};
int glyphOffset(int i) { int o = 0; for (int k = 0; k < i; ++k) o += kGlyphW[k]; return o; }
}

// === HudNode =================================================================

void HudNode::load(Resources* res, const char* assetDir) {
    if (tex_ || !res) return;
    const std::string path = std::string(assetDir) + "/images/hud.png";
    tex_ = res->texture(path);
    res->size(path, texW_, texH_);      // drawn at native size, so it needs both
    // Describe the strip once: eleven variable-width regions, packed left to
    // right. Everything after this asks for "g7" rather than doing division.
    atlas_.init(tex_, texW_, texH_);
    for (int i = 0; i < 11; ++i) {
        char name[8];
        std::snprintf(name, sizeof name, "g%d", i);
        atlas_.add(name, glyphOffset(i), 0, kGlyphW[i], texH_);
    }
}

void HudNode::destroy(IRenderer* r) {
    // Nothing to free: these textures are owned by the engine's Resources
    // cache, which releases them once, after the game shuts down and while
    // the renderer is still alive. Freeing them here too would double-free.
    (void)r;
}

// The digits and the 'm' suffix are variable-width regions packed into one
// strip, which is exactly what otacon::Atlas describes. The UV arithmetic this
// replaced was correct but hand-written in a place that had no business owning
// it -- and the region names now say what is being drawn.
void HudNode::glyph(IRenderer& r, int index, float x, float y) const {
    char name[8];
    std::snprintf(name, sizeof name, "g%d", index);
    atlas_.draw(r, name, x, y);
}

void HudNode::render(IRenderer& r, const Camera&) const {
    if (!tex_) return;
    // Lay the odometer out right to left: the 'm' suffix, then each digit.
    float x = kHudRight - float(kGlyphW[10]);
    glyph(r, 10, x, kHudY);
    int d = distance_;
    if (d == 0) { x -= float(kGlyphW[0]); glyph(r, 0, x, kHudY); return; }
    for (int v = d; v > 0; v /= 10) {
        int digit = v % 10;
        x -= float(kGlyphW[digit]);
        glyph(r, digit, x, kHudY);
    }
}

// === TitleNode ===============================================================

void TitleNode::load(Resources* res, const char* assetDir) {
    if (bgTex_ || !res) return;
    const std::string raw = std::string(assetDir) + "/images/raw/";
    bgTex_ = res->texture(raw + "title.png");
    logoTex_ = res->texture(raw + "title2.png");
    res->size(raw + "title2.png", logoW_, logoH_);
}

void TitleNode::destroy(IRenderer* r) {
    // Nothing to free: these textures are owned by the engine's Resources
    // cache, which releases them once, after the game shuts down and while
    // the renderer is still alive. Freeing them here too would double-free.
    (void)r;
}

void TitleNode::render(IRenderer& r, const Camera&) const {
    const float W = float(kViewW), H = 320.f;
    if (bgTex_) r.drawImage(bgTex_, 0, 0, W, H);        // skyline backdrop
    else        r.fillRect(0, 0, W, H, Color::rgb(0x3a3a44));
    if (logoTex_) r.drawImage(logoTex_, (W - float(logoW_)) * 0.5f, 96.f, float(logoW_), float(logoH_));
    const char* prompt = "PRESS JUMP TO RUN";
    float pw = float(std::strlen(prompt)) * 4.f * 2.f;
    r.drawText(prompt, (W - pw) * 0.5f, 232.f, 2.f, Color{1, 1, 1, 1});
}

// === GameOverNode ============================================================

void GameOverNode::load(Resources* res, const char* assetDir) {
    if (goTex_ || !res) return;
    auto load = [&](const char* f, int& w, int& h) -> TextureHandle {
        const std::string path = std::string(assetDir) + "/images/raw/" + f;
        const TextureHandle t = res->texture(path);
        res->size(path, w, h);
        return t;
    };
    int onW = 0, onH = 0;
    goTex_     = load("gameover.png", goW_, goH_);
    exitTex_   = load("gameover_exit_off.png", exitW_, exitH_);
    exitOnTex_ = load("gameover_exit_on.png", onW, onH);
    recordTex_ = load("gameover_new_record_a.png", recordW_, recordH_);
}

void GameOverNode::destroy(IRenderer* r) {
    // Nothing to free: these textures are owned by the engine's Resources
    // cache, which releases them once, after the game shuts down and while
    // the renderer is still alive. Freeing them here too would double-free.
    (void)r;
}

void GameOverNode::show(int distance, const char* cause, bool newRecord) {
    active_ = true;
    newRecord_ = newRecord;
    distance_ = distance < 0 ? 0 : distance;
    std::snprintf(cause_, sizeof cause_, "%s", cause ? cause : "");
}

void GameOverNode::render(IRenderer& r, const Camera&) const {
    if (!active_) return;
    const float W = float(kViewW);
    const Color dark = Color::rgb(0x35353d);
    const Color white{1, 1, 1, 1};
    constexpr float h = 88.f;            // GAME OVER graphic top (PlayState 'h')

    // Dark bands behind the text + the GAME OVER graphic, then the prompt band.
    r.fillRect(0, h + 35, W, 64, dark);
    r.fillRect(0, h + 35 + 64, W, 2, white);
    r.fillRect(0, float(320 - 30), W, 30, dark);
    if (goTex_) r.drawImage(goTex_, (W - float(goW_)) * 0.5f, h, float(goW_), float(goH_));
    // EXIT button, lit up while the cursor is over it.
    TextureHandle ex = (exitOnTex_ && exitHit(mouseX_, mouseY_)) ? exitOnTex_ : exitTex_;
    if (ex) r.drawImage(ex, W - float(exitW_), 0, float(exitW_), float(exitH_));
    // Beat your best this session? Stamp the NEW RECORD badge above GAME OVER.
    if (newRecord_ && recordTex_)
        r.drawImage(recordTex_, (W - float(recordW_)) * 0.5f, h - float(recordH_) - 6.f,
                    float(recordW_), float(recordH_));

    // Epitaph + retry prompt, centred with the 3x5 bitmap font.
    auto centred = [&](const char* s, float y, float scale) {
        float w = float(std::strlen(s)) * 4.f * scale;
        r.drawText(s, (W - w) * 0.5f, y, scale, white);
    };
    // Turn the short death-cause code into the original's wry epitaph phrasing.
    const char* phrase = "FALLING TO YOUR DEATH";
    if      (std::strcmp(cause_, "bomb") == 0) phrase = "TURNING INTO A FINE MIST";
    else if (std::strcmp(cause_, "hit")  == 0) phrase = "SMASHING INTO A WALL";
    char line[80];
    std::snprintf(line, sizeof line, "YOU RAN %dM", distance_);
    centred(line, h + 48, 2.f);                       // the headline distance
    std::snprintf(line, sizeof line, "BEFORE %s", phrase);
    centred(line, h + 66, 1.f);                       // the wry cause, smaller
    centred("PRESS JUMP OR R TO RUN AGAIN", float(320 - 21), 2.f);
}

} // namespace canabalt
