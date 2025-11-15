#include <chrono>
#include <print>
#include <random>

#include "collision.hpp"
#include "debug.hpp"
#include "engine.hpp"
#include "imgui.h"
#include "render_entity.hpp"
#include "rlgl.h"
#include "sprite.hpp"
#include "sprite_sheet.hpp"
#include "transformer.hpp"

using namespace rlge;

constexpr auto TILES_X = 20;
constexpr auto TILES_Y = 15;
constexpr auto PIXELS_PER_TILE = 8;
constexpr auto MAGNIFICATION = 4;
constexpr auto TILE_PIXELS = PIXELS_PER_TILE * MAGNIFICATION;
constexpr auto SCREEN_PIXELS_X = TILES_X * PIXELS_PER_TILE * MAGNIFICATION;
constexpr auto SCREEN_PIXELS_Y = TILES_Y * PIXELS_PER_TILE * MAGNIFICATION;

struct AppleEaten {
    int amount = 1;
};

struct WallHit {
    int tileX = 0;
    int tileY = 0;
};

class Snake;

class FpsCounter final : public RenderEntity {
public:
    explicit FpsCounter(Scene& scene) :
        RenderEntity(scene) {}

    void draw() override {
        rq().submitUI([] {
            DrawRectangle(5, 5, 80, 30, Fade(BLACK, 0.5f));
            DrawFPS(10, 10);
        });
    }
};

class Scoreboard final : public RenderEntity {
public:
    explicit Scoreboard(Scene& scene, int& score) :
        RenderEntity(scene), score_(score) {}

    void draw() override {
        rq().submitUI([this] {
            const auto text = std::format("Score: {}", score_);
            const auto textWidth = MeasureText(text.c_str(), 30);
            const auto textPosX = SCREEN_PIXELS_X / 2 - textWidth / 2;
            const auto textPosY = 0;
            DrawRectangle(textPosX - 5, textPosY, textWidth + 10, 30, Fade(BLACK, 0.5f));
            DrawText(text.c_str(), textPosX, textPosY, 30, WHITE);
        });
    }
private:
    int& score_;
};

// Background entity that draws a wide patterned image so movement is obvious.
class Background final : public RenderEntity {
public:
    explicit Background(Scene& scene) :
        RenderEntity(scene) {}

    void draw() override {
        if (!visible_)
            return;
        // Draw at world origin; the camera moving will reveal different parts.
        rq().submitBackground([] {
            rlPushMatrix();
            rlTranslatef(0, SCREEN_PIXELS_Y / 2.f, 0); //SCREEN_PIXELS_Y/2.f, 0);
            rlRotatef(90, 1, 0, 0);
            DrawGrid(TILES_X * 2, TILE_PIXELS);
            rlPopMatrix();
        });
    }

private:
    bool visible_ = true;

    friend class GameScene;
};


// Simple player entity that moves on a tile grid
class Snake final : public RenderEntity {
public:
    explicit Snake(Scene& scene, SpriteSheet& sheet) :
        RenderEntity(scene) {
        auto& tr = add<rlge::Transform>();
        tr.scale = {MAGNIFICATION, MAGNIFICATION};

        // Start roughly in the middle of the board in grid coordinates.
        gridX_ = TILES_X / 2;
        gridY_ = TILES_Y / 2;
        prevWorldPos_ = nextWorldPos_ = tileCenter(gridX_, gridY_);
        tr.position = prevWorldPos_;

        sprite_ = &add<SheetSprite>(sheet, 1, 3);

        auto& _ = add<Collider>(
            services().collisions(),
            Rectangle{-TILE_PIXELS / 2.f, -TILE_PIXELS / 2.f, TILE_PIXELS, TILE_PIXELS},
            true);
    }

    void update(float dt) override {
        Entity::update(dt);

        auto& eng = scene().engine();
        const auto& input = eng.input();

        // Change direction discretely based on input and update sprite frame.
        if (input.pressed("left")) {
            dir_ = Direction::Left;
            sprite_->setTile(2, 3);
        }
        else if (input.pressed("right")) {
            dir_ = Direction::Right;
            sprite_->setTile(4, 3);
        }
        else if (input.pressed("up")) {
            dir_ = Direction::Up;
            sprite_->setTile(1, 3);
        }
        else if (input.pressed("down")) {
            dir_ = Direction::Down;
            sprite_->setTile(3, 3);
        }

        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        if (speed_ <= 0.0f)
            return;

        const auto now = std::chrono::steady_clock::now();
        if (now - lastMoveTime_ < moveDuration_)
            return;

        lastMoveTime_ = now;
        auto nextX = gridX_;
        auto nextY = gridY_;
        switch (dir_) {
        case Direction::Left:
            nextX--;
            break;
        case Direction::Right:
            nextX++;
            break;
        case Direction::Up:
            nextY--;
            break;
        case Direction::Down:
            nextY++;
            break;
        default:
            break;
        }
        if (nextX < 0 || nextX >= TILES_X || nextY < 0 || nextY >= TILES_Y)
            return; // Out of bounds, ignore move.

        gridX_ = nextX;
        gridY_ = nextY;
        nextWorldPos_ = tileCenter(gridX_, gridY_);
        tr->position = nextWorldPos_;
    }

private:
    enum class Direction { None, Left, Right, Up, Down };

    float speed_ = 5.0f; // tiles per second
    SheetSprite* sprite_;

    Direction dir_ = Direction::Right;

    // Grid-space position and timing for tile-based movement.
    int gridX_ = 0;
    int gridY_ = 0;

    std::chrono::steady_clock::time_point lastMoveTime_ = std::chrono::steady_clock::now();
    std::chrono::milliseconds moveDuration_ = std::chrono::milliseconds(static_cast<int>(1000.0f / speed_));

    Vector2 prevWorldPos_{0.0f, 0.0f};
    Vector2 nextWorldPos_{0.0f, 0.0f};

    static Vector2 tileCenter(int tileX, int tileY) {
        return {
            static_cast<float>(tileX * TILE_PIXELS) - SCREEN_PIXELS_X / 2.0f + TILE_PIXELS / 2.0f,
            static_cast<float>(tileY * TILE_PIXELS) - SCREEN_PIXELS_Y / 2.0f + TILE_PIXELS / 2.0f
        };
    }

    friend class GameScene;
};

class Wall final : public RenderEntity {
public:
    explicit Wall(Scene& scene, SpriteSheet& sheet, int tileX, int tileY) :
        RenderEntity(scene) {
        rng_ = std::default_random_engine(std::random_device{}());
        tileX_ = tileX;
        tileY_ = tileY;

        auto& tr = add<rlge::Transform>();
        tr.position = {
            static_cast<float>(tileX_ * TILE_PIXELS) - SCREEN_PIXELS_X / 2 + TILE_PIXELS / 2,
            static_cast<float>(tileY_ * TILE_PIXELS) - SCREEN_PIXELS_Y / 2 + TILE_PIXELS / 2
        };
        tr.scale = {4, 4};
        tr.rotation = 90.0f * rotationRng_(rng_);

        sprite_ = &add<SheetSprite>(sheet, sheetSpriteColRng_(rng_), 0);

        auto& coll = add<Collider>(
            services().collisions(),
            Rectangle{-TILE_PIXELS / 2.f, -TILE_PIXELS / 2.f, TILE_PIXELS, TILE_PIXELS},
            true
        );
        coll.setOnCollision([this] (Collider& other) {
            if (dynamic_cast<Snake*>(&other.entity()) == nullptr)
                return;
            events().enqueue(WallHit{(this->tileX_), (this->tileY_)});
        });
    }

private:
    int tileX_, tileY_;
    std::default_random_engine rng_;
    std::uniform_int_distribution<> rotationRng_{0, 3};
    std::uniform_int_distribution<> sheetSpriteColRng_{12, 13};
    SheetSprite* sprite_;
};

class Apple final : public RenderEntity {
public:
    explicit Apple(Scene& scene, SpriteSheet& sheet) :
        RenderEntity(scene)
        , rng_(std::random_device{}())
        , sheet_(sheet) {
        auto& tr = add<rlge::Transform>();
        tr.scale = {MAGNIFICATION, MAGNIFICATION};
        tr.position = randomTilePosition();
        sprite_ = &add<SheetSprite>(sheet_, 6, randomAppleSprite());

        auto& coll = add<Collider>(
            scene.engine().services().collisions(),
            Rectangle{-TILE_PIXELS / 2.f, -TILE_PIXELS / 2.f, TILE_PIXELS, TILE_PIXELS},
            true);

        coll.setOnCollision([this](Collider& other) {
            if (dynamic_cast<Snake*>(&other.entity()) == nullptr)
                return;

            events().enqueue(AppleEaten{1});

            if (auto* t = get<rlge::Transform>()) {
                t->position = randomTilePosition();
                sprite_->setTile(6, randomAppleSprite());
            }
        });
    }

private:
    int randomAppleSprite() {
        std::vector<int> sheetSpriteRow;
        std::sample(sheetSpriteRows_.begin(), sheetSpriteRows_.end(),
                    std::back_inserter(sheetSpriteRow), 1, rng_);
        return sheetSpriteRow.back();
    }
    Vector2 randomTilePosition() {
        const int gx = positionXRng_(rng_);
        const int gy = positionYRng_(rng_);
        return {
            gx * TILE_PIXELS - SCREEN_PIXELS_X / 2.f + TILE_PIXELS / 2.f,
            gy * TILE_PIXELS - SCREEN_PIXELS_Y / 2.f + TILE_PIXELS / 2.f
        };
    }

    std::default_random_engine rng_;
    std::uniform_int_distribution<> positionXRng_{1, TILES_X - 2};
    std::uniform_int_distribution<> positionYRng_{1, TILES_Y - 2};
    std::vector<int> sheetSpriteRows_{0, 1, 3, 4};
    SpriteSheet& sheet_;
    SheetSprite* sprite_{nullptr};
};

class GameScene final : public Scene, public HasDebugOverlay {
public:
    explicit GameScene(Engine& e) :
        Scene(e) {}

    ~GameScene() override {
        auto& bus = engine().services().events();
        if (appleSubId_ != 0) {
            bus.unsubscribe<AppleEaten>(appleSubId_);
        }
    }

    void enter() override {
        // Load background and player sprite generated by Python helpers.
        auto& spriteTex = engine().assetStore().loadTexture("spritesheet", "../examples/snake/assets/spritesheet.png");
        spriteSheet_ = std::make_unique<SpriteSheet>(spriteTex, 8, 8);

        // Draw order: background first, player on top.
        bg_ = &spawn<Background>();
        snake_ = &spawn<Snake>(*spriteSheet_);
        apple_ = &spawn<Apple>(*spriteSheet_);
        scoreBoard_ = &spawn<Scoreboard>(score_);
        fps_ = &spawn<FpsCounter>();

        auto& bus = engine().services().events();
        appleSubId_ = bus.subscribe<AppleEaten>([this](const AppleEaten& e) {
            score_ += e.amount;
        });
        wallSubId_ = bus.subscribe<WallHit>([this](const WallHit& e) {
            std::println("Wall hit!");
            this->engine().quit();
        });

        walls_ = std::vector<Wall*>{};
        for (auto y = 0; y < TILES_Y; ++y) {
            walls_.push_back(&spawn<Wall>(*spriteSheet_, 0, y));
            walls_.push_back(&spawn<Wall>(*spriteSheet_, TILES_X - 1, y));
        }
        for (auto x = 1; x < TILES_X - 1; ++x) {
            walls_.push_back(&spawn<Wall>(*spriteSheet_, x, 0));
            walls_.push_back(&spawn<Wall>(*spriteSheet_, x, TILES_Y - 1));
        }
    }

    void debugOverlay() override {
        ImGui::Begin("Game debug");
        ImGui::Text("Number of entities: %d", static_cast<int>(entities().size()));

        if (snake_) {
            if (auto* tr = snake_->get<rlge::Transform>()) {
                ImGui::SliderFloat("Player X", &tr->position.x, 0, TILES_X * PIXELS_PER_TILE * MAGNIFICATION / 2);
                ImGui::SliderFloat("Player Y", &tr->position.y, 0, TILES_Y * PIXELS_PER_TILE * MAGNIFICATION / 2);
            }
        }
        ImGui::End();
    }

private:
    Background* bg_{nullptr};
    Scoreboard* scoreBoard_{nullptr};
    Snake* snake_{nullptr};
    Apple* apple_{nullptr};
    FpsCounter* fps_{nullptr};
    std::vector<Wall*> walls_;
    std::unique_ptr<SpriteSheet> spriteSheet_{nullptr};
    int score_ = 0;
    EventBus::SubscriptionId appleSubId_{0};
    EventBus::SubscriptionId wallSubId_{0};
};

int main() {
    Engine engine(TILES_X * PIXELS_PER_TILE * MAGNIFICATION, TILES_Y * PIXELS_PER_TILE * MAGNIFICATION, 60,
                  "RLGE Snake");

    // Basic input bindings
    engine.input().bind("left", KEY_A);
    engine.input().bind("right", KEY_D);
    engine.input().bind("up", KEY_W);
    engine.input().bind("down", KEY_S);

    auto& camera = engine.services().camera();
    camera.setOffset({TILES_X * PIXELS_PER_TILE * MAGNIFICATION / 2.0f,
                      TILES_Y * PIXELS_PER_TILE * MAGNIFICATION / 2.0f});
    camera.follow({0.0f, 0.0f}, 1.0f);

    // Start with our game scene
    engine.pushScene<GameScene>();
    engine.run();

    return 0;
}
