#pragma once
#include <string>
#include <unordered_map>

#include "raylib.h"

namespace rlge {
    class AudioManager {
    public:
        AudioManager() { InitAudioDevice(); }

        ~AudioManager() {
            for (auto& kv : sounds_)
                UnloadSound(kv.second);
            for (auto& kv : musics_)
                UnloadMusicStream(kv.second);
            CloseAudioDevice();
        }

        void loadSound(const std::string& id, const std::string& path) {
            if (sounds_.contains(id))
                return;
            sounds_[id] = LoadSound(path.c_str());
        }

        void playSound(const std::string& id) {
            if (auto it = sounds_.find(id); it != sounds_.end())
                PlaySound(it->second);
        }

        void loadMusic(const std::string& id, const std::string& path) {
            if (musics_.contains(id))
                return;
            musics_[id] = LoadMusicStream(path.c_str());
        }

        void playMusic(const std::string& id, bool loop = true) {
            const auto it = musics_.find(id);
            if (it == musics_.end())
                return;
            current_ = &it->second;
            loop_ = loop;
            PlayMusicStream(*current_);
        }

        void stopMusic() {
            if (current_)
                StopMusicStream(*current_);
            current_ = nullptr;
        }

        void update() const {
            if (current_) {
                UpdateMusicStream(*current_);
            }
        }

    private:
        std::unordered_map<std::string, Sound> sounds_;
        std::unordered_map<std::string, Music> musics_;
        Music* current_ = nullptr;
        bool loop_ = true;
    };
}
