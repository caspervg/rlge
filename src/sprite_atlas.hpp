#pragma once

#include <string>
#include <utility>
#include <vector>

#include "raylib.h"

namespace rlge {
    class SpriteAnim;

    class SpriteAtlasBase {
    public:
        virtual ~SpriteAtlasBase() = default;
        virtual void unload() = 0;
    };

    template <typename StateEnum>
    struct AtlasClipSpec {
        StateEnum state;
        std::string path;
        float frameTime{0.1f};
        bool loop{true};
        int startFrame{0};
        int frameCount{0};
    };

    template <typename StateEnum>
    struct AtlasSpec {
        std::string id;
        int frameW{0};
        int frameH{0};
        std::vector<AtlasClipSpec<StateEnum>> clips;
    };

    template <typename StateEnum>
    class SpriteAtlas final : public SpriteAtlasBase {
    public:
        SpriteAtlas(const Texture2D& texture, const int frameW, const int frameH,
                    std::vector<AtlasClipSpec<StateEnum>> clips, std::vector<Rectangle> frames) :
            texture_(texture)
            , frameW_(frameW)
            , frameH_(frameH)
            , clips_(std::move(clips))
            , frames_(std::move(frames)) {}

        ~SpriteAtlas() override { unload(); }

        SpriteAtlas(const SpriteAtlas&) = delete;
        SpriteAtlas& operator=(const SpriteAtlas&) = delete;

        SpriteAtlas(SpriteAtlas&& other) noexcept :
            texture_(other.texture_), frameW_(other.frameW_), frameH_(other.frameH_), clips_(std::move(other.clips_)),
            frames_(std::move(other.frames_)) {
            other.texture_.id = 0;
        }

        SpriteAtlas& operator=(SpriteAtlas&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            unload();
            texture_ = other.texture_;
            frameW_ = other.frameW_;
            frameH_ = other.frameH_;
            clips_ = std::move(other.clips_);
            frames_ = std::move(other.frames_);
            other.texture_.id = 0;
            return *this;
        }

        Texture2D& texture() { return texture_; }
        [[nodiscard]] const Texture2D& texture() const { return texture_; }
        [[nodiscard]] int frameW() const { return frameW_; }
        [[nodiscard]] int frameH() const { return frameH_; }
        [[nodiscard]] const std::vector<AtlasClipSpec<StateEnum>>& clips() const { return clips_; }
        [[nodiscard]] const std::vector<Rectangle>& frames() const { return frames_; }

        void addTo(SpriteAnim& sprite) const;

        void unload() override {
            if (texture_.id != 0) {
                UnloadTexture(texture_);
                texture_.id = 0;
            }
        }

    private:
        Texture2D texture_{};
        int frameW_{0};
        int frameH_{0};
        std::vector<AtlasClipSpec<StateEnum>> clips_;
        std::vector<Rectangle> frames_;
    };
} // namespace rlge
