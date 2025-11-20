#pragma once
#include <string>
#include <unordered_map>

#include "raylib.h"

namespace rlge {
    class AudioManager {
    public:
        AudioManager();
        ~AudioManager();

        void loadSound(const std::string& id, const std::string& path);
        void playSound(const std::string& id);
        void loadMusic(const std::string& id, const std::string& path);
        void playMusic(const std::string& id, bool loop = true);
        void stopMusic();
        void update() const;

    private:
        std::unordered_map<std::string, Sound> sounds_;
        std::unordered_map<std::string, Music> musics_;
        Music* current_ = nullptr;
    };
}
