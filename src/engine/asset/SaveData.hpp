/*
===========================================================================

OTACON ENGINE
asset/SaveData.hpp - a key/value store that survives the process

A high score, a settings choice, which levels are unlocked, where the window
was. Small, structured, and it has to still be there next launch.

Plain text, one `key=value` per line. Binary would be smaller and faster and
neither of those matters for a few dozen keys -- whereas being able to open the
file, read it, and fix a bad value by hand matters every time something goes
wrong. A save format you cannot inspect is a save format you cannot debug.

Two properties worth stating because they are easy to get wrong:

  * Writing is atomic. Write a temporary and rename it over the original, so a
    crash mid-save leaves the previous file intact rather than a truncated one.
    Losing a save to a power cut is forgivable; corrupting it is not.

  * Reading is total. A missing file, a missing key, or a value that will not
    parse all return the caller's default. A save file is user-writable data and
    must never be trusted to be well-formed.

===========================================================================
*/
#pragma once
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace otacon {

class SaveData {
public:
    /*
    ==================
    load

    Returns false when there was no file to read, which is not an error -- it is
    what the first launch looks like. The store is usable either way.
    ==================
    */
    bool load(const char* path) {
        path_ = path ? path : "";
        values_.clear();
        std::FILE* f = std::fopen(path_.c_str(), "rb");
        if (!f) return false;

        std::string line;
        int c;
        while ((c = std::fgetc(f)) != EOF) {
            if (c == '\n' || c == '\r') { absorb(line); line.clear(); continue; }
            if (line.size() < kMaxLine) line.push_back(char(c));
        }
        absorb(line);
        std::fclose(f);
        return true;
    }

    /*
    ==================
    save

    Temp file, then rename. The rename is the atomic step: at no point is the
    real file half-written.
    ==================
    */
    bool save() const {
        if (path_.empty()) return false;
        const std::string tmp = path_ + ".tmp";
        std::FILE* f = std::fopen(tmp.c_str(), "wb");
        if (!f) return false;
        for (const auto& kv : values_)
            std::fprintf(f, "%s=%s\n", kv.first.c_str(), kv.second.c_str());
        std::fclose(f);
        if (std::rename(tmp.c_str(), path_.c_str()) != 0) {
            std::remove(tmp.c_str());
            return false;
        }
        return true;
    }

    // ---- readers: every one takes the value to use when the key is absent ----
    int getInt(const char* key, int fallback = 0) const {
        const std::string* v = find(key);
        if (!v) return fallback;
        char* end = nullptr;
        const long n = std::strtol(v->c_str(), &end, 10);
        return (end && end != v->c_str()) ? int(n) : fallback;
    }
    float getFloat(const char* key, float fallback = 0.f) const {
        const std::string* v = find(key);
        if (!v) return fallback;
        char* end = nullptr;
        const float n = std::strtof(v->c_str(), &end);
        return (end && end != v->c_str()) ? n : fallback;
    }
    bool getBool(const char* key, bool fallback = false) const {
        const std::string* v = find(key);
        if (!v) return fallback;
        return *v == "1" || *v == "true" || *v == "yes";
    }
    std::string getString(const char* key, const char* fallback = "") const {
        const std::string* v = find(key);
        return v ? *v : std::string(fallback ? fallback : "");
    }

    // ---- writers ----
    void set(const char* key, int v)   { values_[key] = std::to_string(v); }
    void set(const char* key, float v) { char b[48]; std::snprintf(b, sizeof b, "%g", double(v)); values_[key] = b; }
    void set(const char* key, bool v)  { values_[key] = v ? "1" : "0"; }
    void set(const char* key, const char* v) { values_[key] = v ? v : ""; }

    // A high score only ever goes up, and getting that wrong is the classic
    // save bug -- so the engine offers the operation rather than the pattern.
    bool raise(const char* key, int value) {
        if (value <= getInt(key, 0)) return false;
        set(key, value);
        return true;
    }

    bool has(const char* key) const { return find(key) != nullptr; }
    void remove(const char* key) { values_.erase(key); }
    void clear() { values_.clear(); }
    std::size_t size() const { return values_.size(); }
    const std::string& path() const { return path_; }

private:
    static constexpr std::size_t kMaxLine = 1024;

    const std::string* find(const char* key) const {
        if (!key) return nullptr;
        const auto it = values_.find(key);
        return it == values_.end() ? nullptr : &it->second;
    }
    // Split on the FIRST '=' so a value may contain one. Anything without a
    // separator, and any comment line, is skipped rather than rejected.
    void absorb(const std::string& line) {
        if (line.empty() || line[0] == '#') return;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) return;
        values_[line.substr(0, eq)] = line.substr(eq + 1);
    }

    std::string path_;
    std::unordered_map<std::string, std::string> values_;
};

} // namespace otacon
