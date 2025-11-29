#include "snake_scene.hpp"

#include <format>

#include "game_over_scene.h"
#include "imgui.h"
#include "particle_emitter.hpp"
#include "particle_fx.hpp"
#include "runtime.hpp"
#include "snake_fx.hpp"
#include "snake_view.hpp"
#include "ui/ui.hpp"

namespace snake {
    class GameOverScene;
    using namespace rlge;

    GameScene::GameScene(Runtime& r) :
        Scene(r), game_(Config{}, &r.services().gameEvents(), &sceneEvents()) {
        forwardGameEvent<SnakeDied>();
    }

    GameScene::~GameScene() {
        auto& bus = sceneEvents();
        if (appleSubId_ != 0) {
            bus.unsubscribe<AppleEaten>(appleSubId_);
        }
        if (diedSubId_ != 0) {
            bus.unsubscribe<SnakeDied>(diedSubId_);
        }
    }

    void GameScene::enter() {
        audio().loadSound("apple", "../examples/snake/assets/apple.wav");
        audio().loadSound("game_over", "../examples/snake/assets/game_over.wav");
        audio().loadMusic("bgm", "../examples/snake/assets/fma-bgm.wav");
        audio().playMusic("bgm", true);

        auto& spriteTex = assets().loadTexture("spritesheet", "../examples/snake/assets/spritesheet.png");
        spriteSheet_ = std::make_unique<SpriteSheet>(spriteTex, kPixelsPerTile, kPixelsPerTile);
        uiFont_ = &assets().loadFont("ui_font", "../examples/snake/assets/fallingskyblack-gyxa.otf", 32);

        camera_ = rlge::Camera();
        camera_.setOffset({snake::kTilesX * snake::kPixelsPerTile * snake::kMagnification / 2.0f,
                           snake::kTilesY * snake::kPixelsPerTile * snake::kMagnification / 2.0f});
        setSingleView(camera_);

        buildHUD_();

        bg_ = &spawn<Background>();
        borders_ = &spawn<BorderTiles>(game_, *spriteSheet_);
        snakeBody_ = &spawn<SnakeBody>(game_, *spriteSheet_);
        snake_ = &spawn<SnakeHead>(game_, *spriteSheet_);
        apple_ = &spawn<AppleSprite>(game_, *spriteSheet_);
        fps_ = &spawn<FpsCounter>();
        // Particle emitter component on the snake head.
        if (snake_) {
            deathFx_ = &snake_->add<BurstParticleEmitter>(deathFxConfig(), deathFxRender());
            deathFx_->burst(80);
        }

        auto& bus = sceneEvents();
        appleSubId_ = bus.subscribe<AppleEaten>([this](const AppleEaten& e) {
            audio().playSound("apple");
            score_ += e.amount;
            if (apple_) {
                apple_->changeSprite();
            }
            // Spawn a transient burst at the eaten apple's position (use event-provided coords).
            spawnBurstEmitter(*this, e.worldPos, 40, appleFxConfig(), appleFxRender(), appleFxSpawn());
        });
        diedSubId_ = bus.subscribe<SnakeDied>([this](const SnakeDied& e) {
            if (deathPending_)
                return;
            audio().playSound("game_over");
            if (deathFx_) {
                deathFx_->burst(deathFx_->burstCount());
            }
            deathPending_ = true;
            deathTimer_ = 0.75f;
        });
    }

    void GameScene::update(const float dt) {
        // Translate input into game directions.
        if (!deathPending_) {
            const auto& input = this->input();
            if (input.pressed(Action::MoveLeft)) {
                game_.setDirection(Direction::Left);
            }
            else if (input.pressed(Action::MoveRight)) {
                game_.setDirection(Direction::Right);
            }
            else if (input.pressed(Action::MoveUp)) {
                game_.setDirection(Direction::Up);
            }
            else if (input.pressed(Action::MoveDown)) {
                game_.setDirection(Direction::Down);
            }

            game_.update(dt);
        }
        else {
            deathTimer_ -= dt;
            if (deathTimer_ <= 0.0f) {
                deathPending_ = false;
                runtime().transitionTo<GameOverScene>(
                    std::make_unique<FadeTransition>(0.3f),
                    score_
                    );
            }
        }

        Scene::update(dt);

        // Auto-clear state once burst is done.
        if (deathFx_ && deathFx_->isDone()) {
            deathFx_->clear();
        }
    }

    void GameScene::exit() { audio().stopMusic(); }

    void GameScene::buildHUD_() {
        auto& uiSystem = ui();
        auto& root = uiSystem.root();
        root.clearChildren();

        const auto [x, _] = runtime().window().size();

        const ui::PanelStyle hudStyle{
            .background = Fade(BLACK, .5f)
        };

        auto& hudPanel = root.addChild<ui::Panel>(
            ui::LayoutConfig{.size = {100.0f, 40.0f}, .padding = {16.0f, 12.0f}},
            hudStyle);

        hudPanel.addChild<ui::HStack>(
                    ui::LayoutConfig{},
                    ui::StackConfig{.spacing = 20.0f, .alignment = ui::Alignment::Center,
                                    .distribution = ui::Distribution::SpaceBetween}
                    )
                .addChild<ui::HStack>(
                    ui::LayoutConfig{},
                    ui::StackConfig{.spacing = 12.0f, .alignment = ui::Alignment::Center}
                    )
                .addChild<ui::Label>(
                    ui::bind([this]() { return std::format("Score: {}", score_); }),
                    ui::LayoutConfig{},
                    ui::LabelStyle{.color = {100, 255, 100, 255}, .fontSize = 24.0f}, uiFont_);
    }


} // namespace snake
