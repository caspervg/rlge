#include "audio.hpp"

namespace rlge {
    AudioManager::AudioManager() {
        InitAudioDevice();
    }

    AudioManager::~AudioManager() {
        for (auto& kv : sounds_)
            UnloadSound(kv.second);
        for (auto& kv : musics_)
            UnloadMusicStream(kv.second);
        CloseAudioDevice();
    }

    void AudioManager::loadSound(const std::string& id, const std::string& path) {
        if (sounds_.contains(id))
            return;
        sounds_[id] = LoadSound(path.c_str());
    }

    void AudioManager::playSound(const std::string& id) {
        if (auto it = sounds_.find(id); it != sounds_.end())
            PlaySound(it->second);
    }

    void AudioManager::loadMusic(const std::string& id, const std::string& path) {
        if (musics_.contains(id))
            return;
        musics_[id] = LoadMusicStream(path.c_str());
    }

    void AudioManager::playMusic(const std::string& id, bool loop) {
        const auto it = musics_.find(id);
        if (it == musics_.end())
            return;
        if (current_) {
            stopMusic();
        }
        current_ = &it->second;
        loop_ = loop;
        PlayMusicStream(*current_);
    }

    void AudioManager::stopMusic() {
        if (current_)
            StopMusicStream(*current_);
        current_ = nullptr;
    }

    void AudioManager::update() const {
        if (current_) {
            UpdateMusicStream(*current_);
        }
    }
}

