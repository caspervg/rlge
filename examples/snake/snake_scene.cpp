#include "snake_scene.hpp"
#include "game_over_scene.h"

#include <format>
#include <random>

#include "imgui.h"
#include "rlgl.h"
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

            // Use a fixed spritesheet tile for body segments.
            const Rectangle src = sheet_.tile(5, 3);
            constexpr auto size = static_cast<float>(kTilePixels);
            constexpr Vector2 origin{size * 0.5f, size * 0.5f};

            for (std::size_t i = 1; i < segments.size(); ++i) {
                const auto [wX, wY] = game_.worldPos(segments[i]);
                const Rectangle dest{
                    wX,
                    wY,
                    size,
                    size
                };
                DrawTexturePro(sheet_.texture(), src, dest, origin, 0.0f, WHITE);
            }
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

    void BorderTiles::draw() {
        for (const auto& tile : tiles_) {
            tile->draw();
        }
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

    GameScene::GameScene(Engine& e) :
        Scene(e)
        , game_(Config{}, &e.services().events()) {}

    GameScene::~GameScene() {
        auto& bus = engine().services().events();
        if (appleSubId_ != 0) {
            bus.unsubscribe<AppleEaten>(appleSubId_);
        }
        if (diedSubId_ != 0) {
            bus.unsubscribe<SnakeDied>(diedSubId_);
        }
    }

    void GameScene::enter() {
        auto& spriteTex = engine().assetStore().loadTexture(
            "spritesheet", "../examples/snake/assets/spritesheet.png");
        spriteSheet_ = std::make_unique<SpriteSheet>(spriteTex, kPixelsPerTile, kPixelsPerTile);

        bg_ = &spawn<Background>();
        borders_ = &spawn<BorderTiles>(game_, *spriteSheet_);
        scoreboard_ = &spawn<Scoreboard>(score_);
        snakeBody_ = &spawn<SnakeBody>(game_, *spriteSheet_);
        snake_ = &spawn<SnakeHead>(game_, *spriteSheet_);
        apple_ = &spawn<AppleSprite>(game_, *spriteSheet_);
        fps_ = &spawn<FpsCounter>();

        auto& bus = engine().services().events();
        appleSubId_ = bus.subscribe<AppleEaten>([this] (const AppleEaten& e) {
            score_ += e.amount;
            if (apple_) {
                apple_->changeSprite();
            }
        });
        diedSubId_ = bus.subscribe<SnakeDied>([this] (const SnakeDied& e) {
            if (scoreboard_) {
                scoreboard_->toggleVisibility();
            }
            engine().pushScene<GameOverScene>(score_);
        });
    }

    void GameScene::update(float dt) {
        // Translate input into game directions.
        const auto& input = engine().input();
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

        Scene::update(dt);
    }

    void GameScene::debugOverlay() {
        ImGui::Begin("Game debug");
        ImGui::Text("Number of entities: %d", static_cast<int>(entities().size()));
        ImGui::Text("Score: %d", score_);
        ImGui::End();
    }

} // namespace snake
