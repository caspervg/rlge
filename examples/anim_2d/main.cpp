#include "action.hpp"

#include <array>
#include <string>

#include "animation.hpp"
#include "asset_atlas.hpp"
#include "raylib.h"
#include "render_entity.hpp"
#include "runtime.hpp"
#include "sprite.hpp"
#include "sprite_atlas_sprite.hpp"
#include "transformer.hpp"
#include "window.hpp"

using namespace rlge;

enum class AnimState {
    Idle = 0,
    Walk = 1,
    Attack = 2,
    Hurt = 3,
    Death = 4
};

namespace {
    constexpr auto kFrameW = 48;
    constexpr auto kFrameH = 48;
    constexpr auto kScale = 3.0f;
    constexpr std::array<int, 6> kVariants{{1, 2, 3, 4, 5, 6}};

    std::string clipPath(const int variant, const char* file) {
        return "../examples/anim_2d/assets/" + std::to_string(variant) + "/" + file;
    }
} // namespace

class AnimatedHero final : public RenderEntity {
public:
    AnimatedHero(Scene& scene, SpriteAtlas<AnimState>& atlas)
        : RenderEntity(scene)
        , atlas_(atlas) {
        auto& tr = add<rlge::Transform>();
        tr.position = {0.0f, 0.0f};
        tr.scale = {kScale, kScale};

        auto& sprite = add<SpriteAnim>(atlas_.texture(), atlas_.frameW(), atlas_.frameH());
        atlas_.addTo(sprite);
        sprite.setAutoAdvance(false);

        anim_ = &add<AnimationStateMachine<AnimState>>(sprite);
        registerClips_();
        anim_->setState(AnimState::Idle);
    }

    void update(const float dt) override {
        handleInput_(dt);
        if (anim_) {
            anim_->update(dt);
        }
    }

    void draw() override {
        if (auto* sprite = get<SpriteAnim>()) {
            sprite->draw();
        }
    }

    [[nodiscard]] AnimState state() const {
        if (!anim_) {
            return AnimState::Idle;
        }
        return anim_->currentState();
    }

private:
    void registerClips_() const {
        if (!anim_) {
            return;
        }
        for (const auto& clip : atlas_.clips()) {
            AnimationClip data{
                .startFrame = clip.startFrame,
                .frameCount = clip.frameCount,
                .frameTime = clip.frameTime,
                .loop = clip.loop
            };
            anim_->registerClip(clip.state, data);
        }
    }

    void handleInput_(float dt) {
        auto* tr = get<rlge::Transform>();
        if (!tr || !anim_) {
            return;
        }

        const auto& input = scene().input();
        const bool attackPressed = input.pressed(anim_demo::Action::Attack);
        const bool movingLeft = input.down(anim_demo::Action::MoveLeft);
        const bool movingRight = input.down(anim_demo::Action::MoveRight);
        const bool hurtPressed = input.pressed(anim_demo::Action::Hurt);
        const bool deathPressed = input.pressed(anim_demo::Action::Death);
        const bool resetPressed = input.pressed(anim_demo::Action::Reset);

        if (anim_->currentState() == AnimState::Death) {
            if (resetPressed) {
                anim_->setState(AnimState::Idle);
            }
            return;
        }

        if (anim_->currentState() == AnimState::Attack || anim_->currentState() == AnimState::Hurt) {
            if (anim_->isFinished()) {
                anim_->setState(AnimState::Idle);
            }
            return;
        }

        if (deathPressed) {
            anim_->setState(AnimState::Death);
            return;
        }

        if (hurtPressed) {
            anim_->setState(AnimState::Hurt);
            return;
        }

        if (attackPressed) {
            anim_->setState(AnimState::Attack);
            return;
        }

        if (movingLeft || movingRight) {
            anim_->setState(AnimState::Walk);
            constexpr auto speed = 60.0f;
            if (movingLeft) {
                facingLeft_ = true;
            } else if (movingRight) {
                facingLeft_ = false;
            }
            tr->position.x += (movingRight ? 1.0f : -1.0f) * speed * dt;
        } else {
            anim_->setState(AnimState::Idle);
        }

        tr->scale.x = facingLeft_ ? -kScale : kScale;
    }

    AnimationStateMachine<AnimState>* anim_{nullptr};
    SpriteAtlas<AnimState>& atlas_;
    bool facingLeft_{false};
};

class Hud final : public RenderEntity {
public:
    Hud(Scene& scene, AnimatedHero& hero)
        : RenderEntity(scene)
        , hero_(hero) {}

    void draw() override {
        auto stateLabel = "Idle";
        switch (hero_.state()) {
            case AnimState::Idle:
                stateLabel = "Idle";
                break;
            case AnimState::Walk:
                stateLabel = "Walk";
                break;
            case AnimState::Attack:
                stateLabel = "Attack";
                break;
            case AnimState::Hurt:
                stateLabel = "Hurt";
                break;
            case AnimState::Death:
                stateLabel = "Death";
                break;
        }

        rq().submitUI([stateLabel] {
            DrawText("Sprite animation demo", 18, 16, 18, RAYWHITE);
            DrawText("A/D: walk, Space: attack, H: hurt, K: death, Enter: reset", 18, 40, 16, RAYWHITE);
            DrawText("Q/E: cycle variant", 18, 64, 16, RAYWHITE);
            DrawText(TextFormat("State: %s", stateLabel), 18, 88, 16, RAYWHITE);
        });
    }

private:
    AnimatedHero& hero_;
};

class AnimDemoScene final : public Scene {
public:
    explicit AnimDemoScene(Runtime& r)
        : Scene(r) {}

    void enter() override {
        const auto [sizeX, sizeY] = runtime().window().size();
        camera_ = Camera2DController();
        camera_.setOffset({sizeX * 0.5f, sizeY * 0.5f});
        setSingleView(camera_);

        loadVariant_(variantIndex_);
    }

    void update(const float dt) override {
        const auto& input = this->input();
        const bool prevVariant = input.pressed(anim_demo::Action::VariantPrev);
        const bool nextVariant = input.pressed(anim_demo::Action::VariantNext);

        if (prevVariant || nextVariant) {
            constexpr auto count = static_cast<int>(kVariants.size());
            const int direction = nextVariant ? 1 : -1;
            variantIndex_ = (variantIndex_ + direction + count) % count;
            loadVariant_(variantIndex_);
        }

        Scene::update(dt);
    }

private:
    void loadVariant_(const int index) {
        const int variant = kVariants.at(static_cast<size_t>(index));
        const std::string atlasId = "anim_demo_atlas_" + std::to_string(variant);
        AtlasSpec<AnimState> spec{
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
        hero_ = &spawn<AnimatedHero>(*atlas_);
        hud_ = &spawn<Hud>(*hero_);
    }

    AnimatedHero* hero_{nullptr};
    Hud* hud_{nullptr};
    Camera2DController camera_;
    SpriteAtlas<AnimState>* atlas_{nullptr};
    int variantIndex_{0};
};

int main() {
    constexpr WindowConfig cfg{
        .width = 960,
        .height = 540,
        .fps = 60,
        .title = "RLGE Animations",
        .debugKey = KeyCode::F12
    };
    Runtime runtime(cfg);

    runtime.input().bind(anim_demo::Action::MoveLeft, KeyCode::A);
    runtime.input().bind(anim_demo::Action::MoveRight, KeyCode::D);
    runtime.input().bind(anim_demo::Action::Attack, KeyCode::Space);
    runtime.input().bind(anim_demo::Action::Hurt, KeyCode::H);
    runtime.input().bind(anim_demo::Action::Death, KeyCode::K);
    runtime.input().bind(anim_demo::Action::Reset, KeyCode::Enter);
    runtime.input().bind(anim_demo::Action::VariantPrev, KeyCode::Q);
    runtime.input().bind(anim_demo::Action::VariantNext, KeyCode::E);

    runtime.pushScene<AnimDemoScene>();
    runtime.run();

    return 0;
}
