/*
===========================================================================

OTACON ENGINE
asset/Resources.hpp - load-once cache for textures and sounds

Without this, loading an asset twice means decoding it twice and holding two GPU
textures, and freeing it means remembering every place it was loaded. Both
problems are the same problem: nothing owns the asset.

So this does. Ask for a path and you get a handle; ask again and you get the
same handle, with no second decode. Everything is released in one call at
shutdown, which is the only way a cache is genuinely easier than not having one.

Keyed on the path as given. Two spellings of the same file are two entries --
resolving that properly needs a real virtual filesystem, and pretending
otherwise would hide the duplicate rather than avoid it.

===========================================================================
*/
#pragma once
#include "asset/Audio.hpp"
#include "asset/Image.hpp"
#include "audio/IAudio.hpp"
#include "render/IRenderer.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace otacon {

class Resources {
public:
    // Neither may be null for the corresponding loader to work; a game with no
    // audio device passes nullptr and sound() then returns 0 throughout.
    void init(IRenderer* renderer, IAudio* audio = nullptr) {
        renderer_ = renderer; audio_ = audio;
    }

    /*
    ==================
    texture

    Decode, upload, remember. A failed load is cached as 0 too -- otherwise a
    missing file is re-read and re-failed every frame something asks for it,
    which turns a cosmetic bug into a performance one.
    ==================
    */
    TextureHandle texture(const std::string& path, bool repeat = false) {
        const auto it = textures_.find(path);
        if (it != textures_.end()) return it->second;
        TextureHandle h = 0;
        if (renderer_) {
            const Image img = loadPng(path.c_str());
            if (img.valid()) h = renderer_->createTexture(img, repeat);
        }
        textures_[path] = h;
        return h;
    }

    SoundId sound(const std::string& path) {
        const auto it = sounds_.find(path);
        if (it != sounds_.end()) return it->second;
        SoundId id = 0;
        if (audio_) {
            const AudioClip clip = loadCaf(path.c_str());
            if (clip.valid()) id = audio_->createSound(clip);
        }
        sounds_[path] = id;
        return id;
    }

    // Upload pixels the caller generated rather than read from disk, under a
    // name of their choosing -- procedural art wants caching too.
    TextureHandle adopt(const std::string& key, const Image& img, bool repeat = false) {
        const auto it = textures_.find(key);
        if (it != textures_.end()) return it->second;
        TextureHandle h = 0;
        if (renderer_ && img.valid()) h = renderer_->createTexture(img, repeat);
        textures_[key] = h;
        return h;
    }

    bool has(const std::string& path) const { return textures_.count(path) != 0; }
    std::size_t textureCount() const { return textures_.size(); }
    std::size_t soundCount() const { return sounds_.size(); }

    /*
    ==================
    shutdown

    Must run while the renderer is still alive, so call it before tearing that
    down. Sounds are owned by the audio device and released with it, so there is
    nothing to destroy here for them.
    ==================
    */
    void shutdown() {
        if (renderer_)
            for (auto& kv : textures_)
                if (kv.second) renderer_->destroyTexture(kv.second);
        textures_.clear();
        sounds_.clear();
    }

private:
    IRenderer* renderer_ = nullptr;
    IAudio*    audio_ = nullptr;
    std::unordered_map<std::string, TextureHandle> textures_;
    std::unordered_map<std::string, SoundId>       sounds_;
};

} // namespace otacon
