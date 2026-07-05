#include "ns_assets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>

namespace neon {

    namespace {
        constexpr int kSampleRate = 44100;

        float frand() { return static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f; }

        // Build a 16-bit mono wave from a sample generator fn(t seconds) -> [-1, 1].
        Wave synth(const float seconds, const std::function<float(float)>& fn) {
            const auto frames = static_cast<unsigned int>(seconds * kSampleRate);
            auto* data = static_cast<std::int16_t*>(RL_MALLOC(frames * sizeof(std::int16_t)));
            for (unsigned int i = 0; i < frames; ++i) {
                const float t = static_cast<float>(i) / kSampleRate;
                float s = std::clamp(fn(t), -1.0f, 1.0f);
                // Short fade at the very end to avoid clicks.
                const float remaining = seconds - t;
                if (remaining < 0.01f)
                    s *= remaining / 0.01f;
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

        float square(const float t, const float hz) { return sine(t, hz) > 0.0f ? 1.0f : -1.0f; }

        float saw(const float t, const float hz) {
            const float p = t * hz;
            return 2.0f * (p - std::floor(p + 0.5f));
        }

        float envExp(const float t, const float rate) { return std::exp(-rate * t); }

        Wave makeLaser() {
            return synth(0.12f, [](const float t) {
                const float hz = 950.0f * std::exp(-9.0f * t) + 180.0f;
                return 0.4f * square(t, hz) * envExp(t, 26.0f);
            });
        }

        Wave makeHit() {
            return synth(0.08f, [](const float t) {
                return 0.5f * frand() * envExp(t, 55.0f);
            });
        }

        Wave makeBoom() {
            return synth(0.6f, [](const float t) {
                const float noise = 0.55f * frand() * envExp(t, 9.0f);
                const float thump = 0.6f * sine(t, 82.0f * std::exp(-2.5f * t) + 34.0f) * envExp(t, 6.0f);
                return noise + thump;
            });
        }

        Wave makeBigBoom() {
            return synth(1.0f, [](const float t) {
                const float noise = 0.6f * frand() * envExp(t, 5.0f);
                const float thump = 0.8f * sine(t, 64.0f * std::exp(-1.8f * t) + 26.0f) * envExp(t, 3.2f);
                return noise + thump;
            });
        }

        Wave makeHurt() {
            return synth(0.28f, [](const float t) {
                const float hz = 220.0f * std::exp(-5.0f * t) + 60.0f;
                return (0.45f * saw(t, hz) + 0.25f * frand()) * envExp(t, 12.0f);
            });
        }

        Wave makePickup() {
            return synth(0.22f, [](const float t) {
                // Quick ascending arpeggio.
                float hz = 660.0f;
                if (t > 0.14f) hz = 1320.0f;
                else if (t > 0.07f) hz = 880.0f;
                return 0.35f * sine(t, hz) * (0.4f + 0.6f * envExp(t, 8.0f));
            });
        }

        Wave makeShield() {
            return synth(0.35f, [](const float t) {
                return 0.25f * (sine(t, 1180.0f) + sine(t, 1560.0f)) * envExp(t, 7.0f);
            });
        }

        Wave makeDash() {
            return synth(0.18f, [](const float t) {
                const float sweep = sine(t, 260.0f + 2600.0f * t);
                return (0.3f * sweep + 0.2f * frand()) * envExp(t, 14.0f);
            });
        }

        Wave makeWaveHorn() {
            return synth(0.55f, [](const float t) {
                const float swell = std::min(t * 8.0f, 1.0f) * envExp(t, 3.5f);
                return 0.28f * (saw(t, 110.0f) + saw(t, 164.8f) + 0.5f * saw(t, 220.0f)) * swell;
            });
        }

        Wave makeGameOver() {
            return synth(1.2f, [](const float t) {
                const float hz = 440.0f * std::exp(-1.4f * t) + 55.0f;
                const float vibrato = 1.0f + 0.01f * sine(t, 6.0f);
                return 0.4f * sine(t, hz * vibrato) * envExp(t, 2.2f);
            });
        }

        Wave makePad() {
            return synth(4.2f, [](const float t) {
                const float tremolo = 0.75f + 0.25f * sine(t, 0.5f);
                const float chord = sine(t, 110.0f) + 0.8f * sine(t, 130.81f) +
                                    0.7f * sine(t, 164.81f) + 0.5f * sine(t, 246.94f);
                const float fadeIn = std::min(t * 2.0f, 1.0f);
                const float fadeOut = std::min((4.2f - t) * 2.0f, 1.0f);
                return 0.08f * chord * tremolo * fadeIn * fadeOut;
            });
        }

        Texture2D makeWhiteTexture() {
            Image img = GenImageColor(4, 4, WHITE);
            Texture2D tex = LoadTextureFromImage(img);
            UnloadImage(img);
            return tex;
        }

        Texture2D makeGlowTexture() {
            Image img = GenImageColor(256, 256, BLANK);
            constexpr float center = 127.5f;
            for (int y = 0; y < 256; ++y) {
                for (int x = 0; x < 256; ++x) {
                    const float dx = (static_cast<float>(x) - center) / center;
                    const float dy = (static_cast<float>(y) - center) / center;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    // Smooth quadratic falloff to fully transparent at the rim.
                    const float a = std::clamp(1.0f - d, 0.0f, 1.0f);
                    const auto alpha = static_cast<unsigned char>(255.0f * a * a);
                    ImageDrawPixel(&img, x, y, Color{255, 255, 255, alpha});
                }
            }
            Texture2D tex = LoadTextureFromImage(img);
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            UnloadImage(img);
            return tex;
        }

        Texture2D makeVignetteTexture() {
            constexpr int size = 512;
            Image img = GenImageColor(size, size, BLANK);
            constexpr float center = (size - 1) * 0.5f;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const float dx = (static_cast<float>(x) - center) / center;
                    const float dy = (static_cast<float>(y) - center) / center;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    const float a = std::clamp((d - 0.55f) / 0.55f, 0.0f, 1.0f);
                    const auto alpha = static_cast<unsigned char>(255.0f * a * a);
                    ImageDrawPixel(&img, x, y, Color{0, 0, 0, alpha});
                }
            }
            Texture2D tex = LoadTextureFromImage(img);
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            UnloadImage(img);
            return tex;
        }
    } // namespace

    SoundBank::~SoundBank() {
        for (auto& [id, entry] : entries_) {
            for (auto& alias : entry.aliases)
                UnloadSoundAlias(alias);
            UnloadSound(entry.base);
        }
    }

    void SoundBank::add(const std::string& id, const Wave& wave, const int voices) {
        // Headless/CI machines may have no audio device; run silently instead of crashing.
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

    void SoundBank::play(const std::string& id, const float volume, const float pitch, const float pitchJitter) {
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

    NsAssets::NsAssets() {
        white = makeWhiteTexture();
        glow = makeGlowTexture();
        vignette = makeVignetteTexture();

        const auto addWave = [this](const std::string& id, Wave w, const int voices = 5) {
            sfx.add(id, w, voices);
            UnloadWave(w);
        };

        addWave("laser", makeLaser(), 8);
        addWave("hit", makeHit(), 8);
        addWave("boom", makeBoom(), 6);
        addWave("bigboom", makeBigBoom(), 3);
        addWave("hurt", makeHurt(), 3);
        addWave("pickup", makePickup(), 4);
        addWave("shield", makeShield(), 3);
        addWave("dash", makeDash(), 4);
        addWave("wavehorn", makeWaveHorn(), 2);
        addWave("gameover", makeGameOver(), 2);
        addWave("pad", makePad(), 2);
    }

    NsAssets::~NsAssets() {
        UnloadTexture(white);
        UnloadTexture(glow);
        UnloadTexture(vignette);
    }

} // namespace neon
