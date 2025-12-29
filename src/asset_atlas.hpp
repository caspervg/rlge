#pragma once

#include <algorithm>
#include <stdexcept>

#include "asset.hpp"
#include "sprite_atlas.hpp"

namespace rlge {
    template <typename StateEnum>
    SpriteAtlas<StateEnum>& AssetStore::loadAtlas(const AtlasSpec<StateEnum>& spec) {
        if (spec.id.empty()) {
            throw std::invalid_argument{"Atlas id cannot be empty"};
        }
        if (spec.frameW <= 0 || spec.frameH <= 0) {
            throw std::invalid_argument{"Atlas frame size must be positive"};
        }
        if (spec.clips.empty()) {
            throw std::invalid_argument{"Atlas must contain at least one clip"};
        }

        if (const auto it = atlases_.find(spec.id); it != atlases_.end()) {
            auto* typed = dynamic_cast<SpriteAtlas<StateEnum>*>(it->second.get());
            if (!typed) {
                throw std::runtime_error{"Atlas id already used with a different state type: " + spec.id};
            }
            return *typed;
        }

        auto clips = spec.clips;
        std::vector<Image> images;
        images.reserve(clips.size());

        int maxWidth = 0;
        int startFrame = 0;

        auto cleanupImages = [&images]() {
            for (const auto& img : images) {
                UnloadImage(img);
            }
            images.clear();
        };

        for (auto& clip : clips) {
            auto imgPath = path(clip.path);
            Image img = LoadImage(imgPath.string().c_str());
            if (img.data == nullptr) {
                cleanupImages();
                throw std::runtime_error{"Failed to load atlas clip image: " + imgPath.string()};
            }
            if (img.width % spec.frameW != 0 || img.width == 0) {
                UnloadImage(img);
                cleanupImages();
                throw std::runtime_error{"Atlas clip width must be a multiple of frameW: " + imgPath.string()};
            }
            if (img.height != spec.frameH) {
                UnloadImage(img);
                cleanupImages();
                throw std::runtime_error{"Atlas clip height must match frameH: " + imgPath.string()};
            }

            const int frameCount = img.width / spec.frameW;
            if (frameCount <= 0) {
                UnloadImage(img);
                cleanupImages();
                throw std::runtime_error{"Atlas clip has no frames: " + imgPath.string()};
            }

            clip.startFrame = startFrame;
            clip.frameCount = frameCount;
            startFrame += frameCount;

            maxWidth = std::max(maxWidth, img.width);
            images.push_back(img);
        }

        Image atlasImage = GenImageColor(maxWidth, spec.frameH * static_cast<int>(clips.size()), BLANK);
        std::vector<Rectangle> frames;
        frames.reserve(startFrame);

        for (size_t row = 0; row < clips.size(); ++row) {
            const auto& clip = clips[row];
            const Image& img = images[row];
            const Rectangle src{0.0f, 0.0f, static_cast<float>(img.width), static_cast<float>(img.height)};
            const Rectangle dst{0.0f, static_cast<float>(row * spec.frameH), src.width, src.height};
            ImageDraw(&atlasImage, img, src, dst, WHITE);

            for (int frame = 0; frame < clip.frameCount; ++frame) {
                frames.push_back(Rectangle{
                    static_cast<float>(frame * spec.frameW),
                    static_cast<float>(row * spec.frameH),
                    static_cast<float>(spec.frameW),
                    static_cast<float>(spec.frameH)
                });
            }
        }

        for (auto& img : images) {
            UnloadImage(img);
        }

        Texture2D atlasTexture = LoadTextureFromImage(atlasImage);
        UnloadImage(atlasImage);

        auto atlas = std::make_unique<SpriteAtlas<StateEnum>>(atlasTexture, spec.frameW, spec.frameH,
                                                              std::move(clips), std::move(frames));
        SpriteAtlas<StateEnum>* ptr = atlas.get();
        atlases_[spec.id] = std::move(atlas);
        return *ptr;
    }

    template <typename StateEnum>
    SpriteAtlas<StateEnum>& AssetStore::atlas(const std::string& id) {
        const auto it = atlases_.find(id);
        if (it == atlases_.end()) {
            throw std::runtime_error{"Atlas not found: " + id};
        }
        auto* typed = dynamic_cast<SpriteAtlas<StateEnum>*>(it->second.get());
        if (!typed) {
            throw std::runtime_error{"Atlas type mismatch: " + id};
        }
        return *typed;
    }
} // namespace rlge
