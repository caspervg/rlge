#include "action.hpp"

#include "anim_demo_scene.hpp"

#include "asset_atlas.hpp"
#include "raylib.h"
#include "render_entity.hpp"
#include "runtime.hpp"
#include "shader_params.hpp"
#include "window.hpp"

namespace {
    class WaterBackdrop final : public rlge::RenderEntity {
    public:
        WaterBackdrop(rlge::Scene& scene, rlge::Camera2DController& camera,
            rlge::ShaderParamsWrapper<anim_demo::WaterParams>* params)
            : RenderEntity(scene)
            , camera_(camera)
            , params_(params) {}

        void draw() override {
            const float zoom = camera_.zoom();
            const float safeZoom = zoom > 0.0f ? zoom : 1.0f;
            const float width = static_cast<float>(GetRenderWidth()) / safeZoom;
            const float height = static_cast<float>(GetRenderHeight()) / safeZoom;
            const auto [targetWidth, targetHeight] = camera_.target();
            const float left = targetWidth - width * 0.5f;
            const float top = targetHeight - height * 0.5f;

            const Shader shader = params_ ? params_->get().shader() : Shader{};
            if (shader.id == 0 || !params_) {
                rq().submitBackground(-1000.0f, [left, top, width, height] {
                    DrawRectangle(static_cast<int>(left), static_cast<int>(top),
                                  static_cast<int>(width), static_cast<int>(height), Color{5, 18, 30, 255});
                });
                return;
            }

            rq().submitCustom(scene().layers().background(), -1000.0f, shader,
                              [left, top, width, height, params = params_] {
                params->apply();
                DrawRectangle(static_cast<int>(left), static_cast<int>(top),
                              static_cast<int>(width), static_cast<int>(height), WHITE);
            });
        }

    private:
        rlge::Camera2DController& camera_;
        rlge::ShaderParamsWrapper<anim_demo::WaterParams>* params_{nullptr};
    };
}

namespace anim_demo {
    AnimDemoScene::AnimDemoScene(rlge::Runtime& r)
        : Scene(r) {}

    void AnimDemoScene::enter() {
        const auto [sizeX, sizeY] = runtime().window().size();
        camera_ = rlge::Camera2DController();
        camera_.setOffset({sizeX * 0.5f, sizeY * 0.5f});
        setSingleView(camera_);

        assets().setRoot(findDemoRoot());
        flashShaderHandle_ = assets().loadFragmentShader("anim_demo_flash", "assets/flash.frag");
        flashShader_ = &assets().shader(flashShaderHandle_);
        waterShaderHandle_ = assets().loadFragmentShader("anim_demo_water_bg", "assets/water_bg.frag");
        waterShader_ = &assets().shader(waterShaderHandle_);
        if (waterShader_ && waterShader_->id != 0) {
            rlge::ShaderParams<WaterParams> params(*waterShader_);
            params.bind("u_time", &WaterParams::time)
                  .bind("u_resolution", &WaterParams::resolution);
            layers().setShaderParams(layers().background(), waterShaderHandle_, std::move(params));
            if (const auto layer = layers().get(layers().background())) {
                waterParams_ = dynamic_cast<rlge::ShaderParamsWrapper<WaterParams>*>(
                    layer->get().shaderParams.get());
            }
        } else {
            layers().clearShader(layers().background());
        }

        backdrop_ = &spawn<WaterBackdrop>(camera_, waterParams_);
        bubbleLayer_ = layers().create("bubble_back", 45, true);

        loadVariant_(variantIndex_);
    }

    void AnimDemoScene::update(const float dt) {
        const auto& input = runtime().input();
        const bool prevVariant = input.pressed(Action::VariantPrev);
        const bool nextVariant = input.pressed(Action::VariantNext);

        if (prevVariant || nextVariant) {
            constexpr auto count = static_cast<int>(kVariants.size());
            const int direction = nextVariant ? 1 : -1;
            variantIndex_ = (variantIndex_ + direction + count) % count;
            loadVariant_(variantIndex_);
        }

        updateWaterParams_(dt);

        Scene::update(dt);
    }

    void AnimDemoScene::updateWaterParams_(const float dt) {
        if (!waterParams_) {
            return;
        }
        auto& [time, resolution] = waterParams_->get().params();
        time += dt;
        resolution = {static_cast<float>(GetRenderWidth()), static_cast<float>(GetRenderHeight())};
    }

    void AnimDemoScene::loadVariant_(const int index) {
        const int variant = kVariants.at(static_cast<size_t>(index));
        const std::string atlasId = "anim_demo_atlas_" + std::to_string(variant);
        const rlge::AtlasSpec<AnimState> spec{
            .id = atlasId,
            .frameW = kFrameW,
            .frameH = kFrameH,
            .clips = {
                {AnimState::Idle, clipPath(variant, "Idle.png"), 0.35f, true},
                {AnimState::Walk, clipPath(variant, "Walk.png"), 0.12f, true},
                {AnimState::Attack, clipPath(variant, "Attack.png"), 0.08f, false},
                {AnimState::Hurt, clipPath(variant, "Hurt.png"), 0.12f, false},
                {AnimState::Death, clipPath(variant, "Death.png"), 0.14f, false}
            }
        };

        atlas_ = &assets().loadAtlas(spec);
        if (hero_) {
            hero_->destroyDeferred();
        }
        if (hud_) {
            hud_->destroyDeferred();
        }
        hero_ = &spawn<SeaCreature>(*atlas_, *flashShader_, flashShaderHandle_, camera_, bubbleLayer_);
        hud_ = &spawn<Hud>(*hero_);
    }
}
