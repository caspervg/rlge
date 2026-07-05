#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"

namespace neon {

    // Small polyphonic sound player built on procedurally synthesized waves.
    // Every effect is generated at startup - the game ships with zero asset files.
    class SoundBank {
    public:
        SoundBank() = default;
        ~SoundBank();

        SoundBank(const SoundBank&) = delete;
        SoundBank& operator=(const SoundBank&) = delete;

        void add(const std::string& id, const Wave& wave, int voices = 5);
        void play(const std::string& id, float volume = 1.0f, float pitch = 1.0f, float pitchJitter = 0.0f);

    private:
        struct Entry {
            Sound base{};
            std::vector<Sound> aliases;
            std::size_t next = 0;
        };

        std::unordered_map<std::string, Entry> entries_;
    };

    // Procedural textures + synthesized SFX shared by all scenes.
    class NsAssets {
    public:
        NsAssets();
        ~NsAssets();

        NsAssets(const NsAssets&) = delete;
        NsAssets& operator=(const NsAssets&) = delete;

        Texture2D white{};     // tiny white quad (also used as nebula canvas)
        Texture2D glow{};      // soft radial glow
        Texture2D vignette{};  // dark screen-edge falloff

        SoundBank sfx;
    };

} // namespace neon
