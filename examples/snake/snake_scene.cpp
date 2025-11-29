#include "snake_scene.hpp"

#include <algorithm>
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

    GameScene::GameScene(Runtime& r) : Scene(r), game_(Config{}, &r.services().gameEvents(), &sceneEvents()) {
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

        camera_ = rlge::Camera();
        camera_.setOffset({snake::kTilesX * snake::kPixelsPerTile * snake::kMagnification / 2.0f,
                           snake::kTilesY * snake::kPixelsPerTile * snake::kMagnification / 2.0f});
        setSingleView(camera_);

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

        // Build HUD using the UI system.
        auto& ui = runtime().services().ui();
        auto& root = ui.root();
        root.clearChildren();

        const auto windowSize = runtime().window().size();

        const ui::PanelStyle hudStyle{
            .background = Fade(BLACK, 180),
            .border = {0, 0, 0, 0},
            .borderThickness = 0.0f
        };

        auto& hudPanel = root.addChild<ui::Panel>(
            ui::LayoutConfig{.size = {windowSize.x, 48.0f}, .padding = {12.0f, 10.0f}},
            hudStyle);

        auto& bar = hudPanel.addChild<ui::HStack>(
            ui::LayoutConfig{},
            ui::StackConfig{.spacing = 12.0f, .alignment = ui::Alignment::Center, .distribution = ui::Distribution::SpaceBetween});

        auto& left = bar.addChild<ui::HStack>(
            ui::LayoutConfig{},
            ui::StackConfig{.spacing = 10.0f, .alignment = ui::Alignment::Center});
        left.addChild<ui::Label>(
            ui::bind([this]() { return std::format("Score: {}", score_); }),
            ui::LayoutConfig{},
            ui::LabelStyle{.color = WHITE, .fontSize = 20.0f});
        left.addChild<ui::Label>(
            ui::bind([this]() { return std::format("Length: {}", game_.body().size()); }),
            ui::LayoutConfig{},
            ui::LabelStyle{.color = LIGHTGRAY, .fontSize = 18.0f});

        auto& right = bar.addChild<ui::HStack>(
            ui::LayoutConfig{},
            ui::StackConfig{.spacing = 8.0f, .alignment = ui::Alignment::Center});
        auto& quitBtn = right.addChild<ui::Button>(
            "Quit",
            ui::LayoutConfig{.padding = {10.0f, 6.0f}},
            ui::ButtonStyle{
                .background = {80, 40, 40, 220},
                .hover = {100, 60, 60, 240},
                .pressed = {60, 30, 30, 255},
                .disabled = {40, 40, 40, 200},
                .text = WHITE,
                .fontSize = 18.0f});
        quitBtn.setId("snake_quit");
        quitBtn.setOnClick([this] { runtime().quit(); });
    }

    void GameScene::exit() { audio().stopMusic(); }

    void GameScene::debugOverlay() {
        ImGui::Begin("Game debug");
        ImGui::Text("Number of entities: %d", static_cast<int>(entities().size()));
        ImGui::Text("Score: %d", score_);
        ImGui::End();
    }

} // namespace snake
