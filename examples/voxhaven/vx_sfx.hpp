#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"

namespace vox {

    // Polyphonic player for procedurally synthesized sounds (zero asset files).
    // Degrades to silence when no audio device is available (headless/CI).
    class Sfx {
    public:
        Sfx();
        ~Sfx();

        Sfx(const Sfx&) = delete;
        Sfx& operator=(const Sfx&) = delete;

        void play(const std::string& id, float volume = 1.0f, float pitch = 1.0f,
                  float pitchJitter = 0.0f);

    private:
        struct Entry {
            Sound base{};
            std::vector<Sound> aliases;
            std::size_t next = 0;
        };

        void add(const std::string& id, const Wave& wave, int voices);

        std::unordered_map<std::string, Entry> entries_;
    };

} // namespace vox
