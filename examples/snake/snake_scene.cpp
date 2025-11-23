#include "snake_scene.hpp"

#include <algorithm>

#include "game_over_scene.h"

#include <format>
#include <print>
#include <random>

#include "imgui.h"
#include "particle_emitter.hpp"
#include "particle_fx.hpp"
#include "rlgl.h"
#include "runtime.hpp"
#include "transformer.hpp"

namespace snake {
    class GameOverScene;
    using namespace rlge;

    void FpsCounter::draw() {
        rq().submitUI([] {
            DrawRectangle(5, 5, 80, 30, Fade(BLACK, 0.5f));
            DrawFPS(10, 10);
        });
    }

    void Background::draw() {
        if (!visible_)
            return;

        rq().submitBackground([] {
            rlPushMatrix();
            rlTranslatef(0, kScreenPixelsY / 2.0f, 0);
            rlRotatef(90, 1, 0, 0);
            DrawGrid(kTilesX * 2, kTilePixels);
            rlPopMatrix();
        });
    }

    SnakeHead::SnakeHead(Scene& scene, Game& game, SpriteSheet& sheet) :
        RenderEntity(scene)
        , game_(game) {
        auto& tr = add<rlge::Transform>();
        tr.scale = {static_cast<float>(kMagnification), static_cast<float>(kMagnification)};
        tr.position = game_.headWorldPos();

        sprite_ = &add<SheetSprite>(sheet, 1, 3);
    }

    void SnakeHead::update(float dt) {
        RenderEntity::update(dt);

        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        tr->position = game_.headWorldPos();

        switch (game_.direction()) {
        case Direction::Left:
            sprite_->setTile(2, 3);
            break;
        case Direction::Right:
            sprite_->setTile(4, 3);
            break;
        case Direction::Up:
            sprite_->setTile(1, 3);
            break;
        case Direction::Down:
            sprite_->setTile(3, 3);
            break;
        }
    }

    SnakeBody::SnakeBody(Scene& scene, Game& game, SpriteSheet& sheet) :
        RenderEntity(scene)
        , game_(game)
        , sheet_(sheet) {}

    void SnakeBody::draw() {
        rq().submitWorld([this] {
            const auto& segments = game_.body();
            if (segments.size() <= 1)
                return;

            constexpr auto size = static_cast<float>(kTilePixels);
            constexpr Vector2 origin{size * 0.5f, size * 0.5f};

            for (std::size_t i = 1; i < segments.size(); ++i) {
                // Regular body part
                Rectangle src = sheet_.tile(5, 3);
                auto rot = 0.0f;

                // Tail
                if (i == segments.size() - 1) {
                    src = sheet_.tile(8, 3);

                    const auto myCell = segments[i];
                    const auto prevCell = segments[i - 1];
                    if (myCell.x < prevCell.x) {
                        rot = 270.f; // facing left
                    }
                    else if (myCell.x > prevCell.x) {
                        rot = 90.0f; // facing right
                    }
                    else if (myCell.y < prevCell.y) {
                        rot = 0.0f; // facing up
                    }
                    else if (myCell.y > prevCell.y) {
                        rot = 180.0f; // facing down
                    }
                }

                const auto [wX, wY] = game_.worldPos(segments[i]);
                const Rectangle dest{
                    wX,
                    wY,
                    size,
                    size
                };
                DrawTexturePro(sheet_.texture(), src, dest, origin, rot, WHITE);
            }
        });
    }

    BorderTile::BorderTile(Scene& scene, Game& game, SpriteSheet& sheet, int xg, int yg) :
        RenderEntity(scene)
        , sheet_(sheet), game_(game)
        , xg_(xg)
        , yg_(yg) {

        std::uniform_int_distribution<> rotationRng_{0, 3};
        std::uniform_int_distribution<> sheetSpriteColRng_{12, 13};
        rotation_ = rotationRng_(*game.rng());
        spriteCol_ = sheetSpriteColRng_(*game.rng());
    }

    void BorderTile::draw() {
        rq().submitWorld([this] {
            constexpr auto size = static_cast<float>(kTilePixels);
            const auto [wX, wY] = game_.worldPos(Game::Cell{xg_, yg_});
            const Rectangle src = sheet_.tile(spriteCol_, 0);
            constexpr Vector2 origin{size * 0.5f, size * 0.5f};
            const Rectangle dest{
                wX,
                wY,
                size,
                size
            };
            DrawTexturePro(sheet_.texture(), src, dest, origin, 90.0f * rotation_, WHITE);
        });
    }

    BorderTiles::BorderTiles(Scene& scene, Game& game, SpriteSheet& sheet) :
        RenderEntity(scene) {
        tiles_ = std::vector<std::unique_ptr<BorderTile>>();

        for (auto y = 0; y < kTilesY; ++y) {
            tiles_.push_back(std::make_unique<BorderTile>(scene, game, sheet, 0, y));
            tiles_.push_back(std::make_unique<BorderTile>(scene, game, sheet, kTilesX - 1, y));
        }
        for (auto x = 1; x < kTilesX - 1; ++x) {
            tiles_.push_back(std::make_unique<BorderTile>(scene, game, sheet, x, 0));
            tiles_.push_back(std::make_unique<BorderTile>(scene, game, sheet, x, kTilesY - 1));
        }
    }

    void BorderTiles::draw() {
        for (const auto& tile : tiles_) {
            tile->draw();
        }
    }

    AppleSprite::AppleSprite(Scene& scene, Game& game, SpriteSheet& sheet) :
        RenderEntity(scene)
        , game_(game) {
        auto& tr = add<rlge::Transform>();
        tr.scale = {static_cast<float>(kMagnification), static_cast<float>(kMagnification)};
        tr.position = game_.appleWorldPos();

        sprite_ = &add<SheetSprite>(sheet, 6, 3);
    }

    void AppleSprite::update(float dt) {
        RenderEntity::update(dt);

        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        tr->position = game_.appleWorldPos();
    }

    void AppleSprite::changeSprite() const {
        sprite_->setTile(6, randomSpriteRow());
    }

    int AppleSprite::randomSpriteRow() const {
        std::vector<int> samples;
        std::ranges::sample(sheetSpriteRows_,
                            std::back_inserter(samples), 1, *game_.rng());
        return samples.back();
    }

    void Scoreboard::draw() {
        if (!visible_) return;

        rq().submitUI([this] {
            const auto text = std::format("Score: {}", score_);
            const auto textWidth = MeasureText(text.c_str(), 30);
            const auto textPosX = kScreenPixelsX / 2 - textWidth / 2;
            const auto textPosY = 0;
            DrawRectangle(textPosX - 5, textPosY, textWidth + 10, 30, Fade(BLACK, 0.5f));
            DrawText(text.c_str(), textPosX, textPosY, 30, WHITE);
        });
    }

    GameScene::GameScene(Runtime& r) :
        Scene(r)
        , game_(Config{}, &r.services().gameEvents(), &sceneEvents()) {
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

        auto& spriteTex = assets().loadTexture(
            "spritesheet", "../examples/snake/assets/spritesheet.png");
        spriteSheet_ = std::make_unique<SpriteSheet>(spriteTex, kPixelsPerTile, kPixelsPerTile);

        camera_ = rlge::Camera();
        camera_.setOffset({
            snake::kTilesX * snake::kPixelsPerTile * snake::kMagnification / 2.0f,
            snake::kTilesY * snake::kPixelsPerTile * snake::kMagnification / 2.0f
        });
        setSingleView(camera_);

        bg_ = &spawn<Background>();
        borders_ = &spawn<BorderTiles>(game_, *spriteSheet_);
        scoreboard_ = &spawn<Scoreboard>(score_);
        snakeBody_ = &spawn<SnakeBody>(game_, *spriteSheet_);
        snake_ = &spawn<SnakeHead>(game_, *spriteSheet_);
        apple_ = &spawn<AppleSprite>(game_, *spriteSheet_);
        fps_ = &spawn<FpsCounter>();
        // Particle emitter component on the snake head.
        if (snake_) {
            deathFx_ = &snake_->add<rlge::BurstParticleEmitter>(rlge::BurstEmitterConfig{}, [](const rlge::Particle& p) {
                // Velocity-aligned streak for death sparks.
                const float speed = Vector2Length(p.vel);
                if (speed < 1e-3f) {
                    DrawCircleV(p.pos, p.size, p.color);
                    return;
                }
                const float len = std::max(6.0f, speed * 0.01f);
                const Vector2 dir = Vector2Normalize(p.vel);
                const Vector2 tail{p.pos.x - dir.x * len, p.pos.y - dir.y * len};
                DrawLineEx(tail, p.pos, p.size * 0.7f, p.color);
            });
            deathFx_->setBurstCount(80);
            deathFx_->setLifetimeRange(0.2f, 0.5f);
            deathFx_->setSpeedRange(200.0f, 520.0f);
            deathFx_->setSizeRange(2.0f, 5.0f);
            deathFx_->setSpread(2.0f * PI);
            deathFx_->setGravity({0.0f, 550.0f});
            deathFx_->setColorRange({255, 120, 60, 255}, Fade(ORANGE, 0.0f));
        }

        auto& bus = sceneEvents();
        appleSubId_ = bus.subscribe<AppleEaten>([this] (const AppleEaten& e) {
            audio().playSound("apple");
            score_ += e.amount;
            if (apple_) {
                apple_->changeSprite();
            }
            // Spawn a transient burst at the eaten apple's position.
            BurstEmitterConfig fxCfg;
            fxCfg.maxParticles = 60;
            fxCfg.minLifetime = 0.25f;
            fxCfg.maxLifetime = 0.6f;
            fxCfg.minSpeed = 40.0f;
            fxCfg.maxSpeed = 120.0f;
            fxCfg.minSize = 2.0f;
            fxCfg.maxSize = 6.0f;
            fxCfg.spread = 2.0f * PI;
            fxCfg.gravity = {0.0f, -60.0f};
            fxCfg.startColor = GREEN;
            fxCfg.endColor = Fade(LIME, 0.0f);
            spawnBurstEmitter(
                *this,
                game_.appleWorldPos(),
                40,
                fxCfg,
                [](const rlge::Particle& p) { DrawCircleV(p.pos, p.size, p.color); },
                [](const Vector2 origin) {
                    return spawnInBox(origin, static_cast<float>(kTilePixels) * 0.25f, static_cast<float>(kTilePixels) * 0.25f);
                });
        });
        diedSubId_ = bus.subscribe<SnakeDied>([this] (const SnakeDied& e) {
            if (deathPending_)
                return;
            audio().playSound("game_over");
            if (deathFx_) {
                deathFx_->burst(deathFx_->burstCount());
            }
            if (scoreboard_) {
                scoreboard_->toggleVisibility();
            }
            deathPending_ = true;
            deathTimer_ = 0.75f;
        });
    }

    void GameScene::update(const float dt) {
        // Translate input into game directions.
        if (!deathPending_) {
            const auto& input = this->input();
            if (input.pressed("left")) {
                game_.setDirection(Direction::Left);
            }
            else if (input.pressed("right")) {
                game_.setDirection(Direction::Right);
            }
            else if (input.pressed("up")) {
                game_.setDirection(Direction::Up);
            }
            else if (input.pressed("down")) {
                game_.setDirection(Direction::Down);
            }

            game_.update(dt);
        } else {
            deathTimer_ -= dt;
            if (deathTimer_ <= 0.0f) {
                deathPending_ = false;
                runtime().pushScene<GameOverScene>(score_);
                return;
            }
        }

        Scene::update(dt);

        // Auto-clear state once burst is done.
        if (deathFx_ && deathFx_->isDone()) {
            deathFx_->clear();
        }
    }

    void GameScene::exit() {
        audio().stopMusic();
    }

    void GameScene::debugOverlay() {
        ImGui::Begin("Game debug");
        ImGui::Text("Number of entities: %d", static_cast<int>(entities().size()));
        ImGui::Text("Score: %d", score_);
        ImGui::End();
    }

} // namespace snake
