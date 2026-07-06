#include "vx_sfx.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

namespace vox {

    namespace {
        constexpr int kSampleRate = 44100;

        float frand() { return static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f; }

        Wave synth(const float seconds, const std::function<float(float)>& fn) {
            const auto frames = static_cast<unsigned int>(seconds * kSampleRate);
            auto* data = static_cast<std::int16_t*>(RL_MALLOC(frames * sizeof(std::int16_t)));
            for (unsigned int i = 0; i < frames; ++i) {
                const float t = static_cast<float>(i) / kSampleRate;
                float s = std::clamp(fn(t), -1.0f, 1.0f);
                const float remaining = seconds - t;
                if (remaining < 0.012f)
                    s *= remaining / 0.012f;
                data[i] = static_cast<std::int16_t>(s * 32000.0f);
            }
            Wave w{};
            w.frameCount = frames;
            w.sampleRate = kSampleRate;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = data;
            return w;
        }

        float sine(const float t, const float hz) { return std::sin(2.0f * PI * hz * t); }

        float envExp(const float t, const float rate) { return std::exp(-rate * t); }

        // Soft one-pole lowpass over white noise gives a "thud" texture.
        struct NoiseLp {
            float state = 0.0f;
            float operator()(const float alpha) {
                state += alpha * (frand() - state);
                return state;
            }
        };

        Wave makeDig() {
            auto lp = std::make_shared<NoiseLp>();
            return synth(0.09f, [lp](const float t) {
                return 2.4f * (*lp)(0.30f) * envExp(t, 40.0f);
            });
        }

        Wave makeBreak() {
            auto lp = std::make_shared<NoiseLp>();
            return synth(0.18f, [lp](const float t) {
                const float crunch = 2.2f * (*lp)(0.45f) * envExp(t, 22.0f);
                const float knock = 0.4f * sine(t, 140.0f * std::exp(-8.0f * t) + 60.0f) * envExp(t, 18.0f);
                return crunch + knock;
            });
        }

        Wave makePlace() {
            return synth(0.07f, [](const float t) {
                return (0.5f * sine(t, 320.0f) + 0.3f * frand()) * envExp(t, 45.0f);
            });
        }

        Wave makeSplash() {
            auto lp = std::make_shared<NoiseLp>();
            return synth(0.35f, [lp](const float t) {
                const float swell = std::min(t * 20.0f, 1.0f);
                return 1.8f * (*lp)(0.5f) * swell * envExp(t, 9.0f);
            });
        }

        Wave makeJump() {
            return synth(0.09f, [](const float t) {
                return 0.25f * sine(t, 180.0f + 500.0f * t) * envExp(t, 26.0f);
            });
        }

        Wave makeChime() {
            return synth(1.1f, [](const float t) {
                float hz1 = 523.25f;
                float hz2 = 659.25f;
                if (t > 0.35f) { hz1 = 659.25f; hz2 = 783.99f; }
                return 0.16f * (sine(t, hz1) + 0.7f * sine(t, hz2)) * envExp(t, 3.0f);
            });
        }

        Wave makeWind() {
            auto lp = std::make_shared<NoiseLp>();
            return synth(3.6f, [lp](const float t) {
                const float breathe = 0.55f + 0.45f * sine(t, 0.35f);
                const float fadeIn = std::min(t * 1.2f, 1.0f);
                const float fadeOut = std::min((3.6f - t) * 1.2f, 1.0f);
                return 0.55f * (*lp)(0.045f) * breathe * fadeIn * fadeOut;
            });
        }
    } // namespace

    Sfx::Sfx() {
        const auto addWave = [this](const std::string& id, Wave w, const int voices) {
            add(id, w, voices);
            UnloadWave(w);
        };
        addWave("dig", makeDig(), 6);
        addWave("break", makeBreak(), 6);
        addWave("place", makePlace(), 6);
        addWave("splash", makeSplash(), 3);
        addWave("jump", makeJump(), 3);
        addWave("chime", makeChime(), 2);
        addWave("wind", makeWind(), 2);
    }

    Sfx::~Sfx() {
        for (auto& [id, entry] : entries_) {
            for (auto& alias : entry.aliases)
                UnloadSoundAlias(alias);
            UnloadSound(entry.base);
        }
    }

    void Sfx::add(const std::string& id, const Wave& wave, const int voices) {
        if (!IsAudioDeviceReady())
            return;
        Entry entry;
        entry.base = LoadSoundFromWave(wave);
        if (entry.base.frameCount == 0 || entry.base.stream.buffer == nullptr) {
            UnloadSound(entry.base);
            return;
        }
        for (int i = 0; i < voices; ++i)
            entry.aliases.push_back(LoadSoundAlias(entry.base));
        entries_.emplace(id, std::move(entry));
    }

    void Sfx::play(const std::string& id, const float volume, const float pitch, const float pitchJitter) {
        const auto it = entries_.find(id);
        if (it == entries_.end())
            return;
        auto& entry = it->second;
        if (entry.aliases.empty())
            return;
        Sound& voice = entry.aliases[entry.next];
        entry.next = (entry.next + 1) % entry.aliases.size();
        const float jitter = pitchJitter > 0.0f ? 1.0f + pitchJitter * frand() : 1.0f;
        SetSoundVolume(voice, volume);
        SetSoundPitch(voice, pitch * jitter);
        PlaySound(voice);
    }

} // namespace vox
