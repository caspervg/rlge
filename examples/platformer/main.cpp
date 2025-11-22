#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "raylib.h"
#include "render_entity.hpp"
#include "runtime.hpp"
#include "sprite_sheet.hpp"
#include "tilemap.hpp"
#include "transformer.hpp"
#include "window.hpp"

#include <tileson.hpp>

namespace demo {
    constexpr int kWindowWidth = 20 * 80;
    constexpr int kWindowHeight = 18 * 80;

    constexpr float kTileSize = 8.0f;
    constexpr float kCameraZoom = 10.0f;
    constexpr float kLevelHideOffset = 8000.0f;

    constexpr Color kSkyTop{20, 28, 46, 255};
    constexpr Color kSkyBottom{8, 12, 22, 255};

    constexpr int kGidBackground = 5;
    constexpr int kGidSpawn = 1;
    constexpr int kGidSpawnAlt = 3;
    constexpr int kGidExit = 10;
    constexpr int kGidCoin = 6;
    constexpr int kGidHazard = 7;

    struct LevelDef {
        std::string name;
        std::filesystem::path path;
    };

    struct LevelState {
        LevelDef def;
        int width{0};
        int height{0};
        int tileWidth{static_cast<int>(kTileSize)};
        int tileHeight{static_cast<int>(kTileSize)};
        std::vector<std::uint8_t> solidMask;
        std::vector<Vector2> coins;
        std::vector<std::uint8_t> coinCollected;
        std::vector<Rectangle> hazards;
        Rectangle exitRect{0, 0, kTileSize, kTileSize};
        Vector2 spawn{0, 0};
        rlge::Tilemap* tilemap{nullptr};

        int resetCoins() {
            int restored = 0;
            for (auto& collected : coinCollected) {
                if (collected) {
                    collected = 0;
                    ++restored;
                }
            }
            return restored;
        }

        [[nodiscard]] int totalCoins() const {
            return static_cast<int>(coins.size());
        }
    };

    class GameScene;

    class BackgroundLayer final : public rlge::RenderEntity {
    public:
        BackgroundLayer(rlge::Scene& scene, Color top, Color bottom);
        void draw() override;
    private:
        Color top_;
        Color bottom_;
    };

    class CoinField final : public rlge::RenderEntity {
    public:
        CoinField(rlge::Scene& scene, GameScene& game, rlge::SpriteSheet& sheet);
        void update(float dt) override;
        void draw() override;
    private:
        GameScene& game_;
        rlge::SpriteSheet& sheet_;
        float timer_{0.0f};
        Rectangle coinSrc_{};
    };

    class GoalMarker final : public rlge::RenderEntity {
    public:
        GoalMarker(rlge::Scene& scene, GameScene& game, rlge::SpriteSheet& sheet);
        void update(float dt) override;
        void draw() override;
    private:
        GameScene& game_;
        rlge::SpriteSheet& sheet_;
        float pulse_{0.0f};
        Rectangle goalSrc_{};
    };

    class Hud final : public rlge::RenderEntity {
    public:
        Hud(rlge::Scene& scene, GameScene& game);
        void draw() override;
    private:
        GameScene& game_;
        Font font_{GetFontDefault()};
    };

    class Player final : public rlge::RenderEntity {
    public:
        Player(rlge::Scene& scene, GameScene& game, rlge::SpriteSheet& sheet);

        void update(float dt) override;
        void respawn(Vector2 pos);
        void setControlEnabled(bool enabled);
        [[nodiscard]] Vector2 position() const;
        [[nodiscard]] Rectangle bounds() const;

    private:
        GameScene& game_;
        rlge::Transform* transform_{nullptr};
        rlge::SheetSprite* sprite_{nullptr};
        Vector2 velocity_{0, 0};
        bool onGround_{false};
        bool controlEnabled_{true};
        bool facingRight_{true};
        float coyoteTimer_{0.0f};
        float jumpBuffer_{0.0f};
        int extraJumps_{1};

        Rectangle computeBounds(Vector2 pos) const;
        void integrateHorizontal(Rectangle& box, float dt);
        void integrateVertical(Rectangle& box, float dt);
    };

    class TitleCard final : public rlge::RenderEntity {
    public:
        TitleCard(rlge::Scene& scene);
        void update(float dt) override;
        void draw() override;
    private:
        float timer_{0.0f};
    };

    class GameScene final : public rlge::Scene {
    public:
        explicit GameScene(rlge::Runtime& runtime, int startingLevel = 0);

        void enter() override;
        void update(float dt) override;

        [[nodiscard]] LevelState& currentLevel();
        [[nodiscard]] const LevelState& currentLevel() const;
        [[nodiscard]] bool isSolid(int tx, int ty) const;
        [[nodiscard]] bool checkHazard(const Rectangle& rect) const;
        [[nodiscard]] bool checkGoal(const Rectangle& rect) const;
        int collectCoins(const Rectangle& rect);

        void handlePlayerDeath();
        void completeLevel();
        void restartLevel();

        [[nodiscard]] bool canControlPlayer() const;
        [[nodiscard]] bool isTransitioning() const { return transitioning_; }
        [[nodiscard]] bool isRespawning() const { return respawning_; }
        [[nodiscard]] bool runComplete() const { return runComplete_; }
        [[nodiscard]] Vector2 spawnPoint() const { return currentLevel().spawn; }
        [[nodiscard]] Rectangle exitRect() const { return currentLevel().exitRect; }
        [[nodiscard]] Vector2 exitCenter() const {
            const auto& rect = currentLevel().exitRect;
            return Vector2{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
        }
        [[nodiscard]] int levelIndex() const { return currentLevel_; }
        [[nodiscard]] int levelCount() const { return static_cast<int>(levels_.size()); }
        [[nodiscard]] int coinsCollected() const { return totalCoinsCollected_; }
        [[nodiscard]] int totalCoinsGoal() const { return totalCoins_; }
        [[nodiscard]] int deathCount() const { return deaths_; }
        [[nodiscard]] Vector2 playerPosition() const;
        [[nodiscard]] rlge::SpriteSheet& sheet();

    private:
        friend class CoinField;
        friend class GoalMarker;
        friend class Hud;
        friend class Player;

        void buildLevels();
        LevelState loadLevel(const LevelDef& def);
        void activateLevel(int index);
        void advanceLevel();
        void updateCamera(float dt);

        const std::array<LevelDef, 2> levelDefs_{{
            {"Overgrown Ruins", "../examples/platformer/assets/levels/level1.tmj"},
            {"Crystal Cavern", "../examples/platformer/assets/levels/level2.tmj"}
        }};

        std::vector<LevelState> levels_;
        int requestedLevel_{0};
        int currentLevel_{0};
        Texture2D* tileset_{nullptr};
        std::unique_ptr<rlge::SpriteSheet> spriteSheet_;
        BackgroundLayer* background_{nullptr};
        CoinField* coinField_{nullptr};
        GoalMarker* goal_{nullptr};
        Hud* hud_{nullptr};
        Player* player_{nullptr};

        int totalCoins_{0};
        int totalCoinsCollected_{0};
        int deaths_{0};
        bool respawning_{false};
        float respawnTimer_{0.0f};
        bool transitioning_{false};
        float transitionTimer_{0.0f};
        bool runComplete_{false};
        float time_{0.0f};
    };

    class TitleScene final : public rlge::Scene {
    public:
        using Scene::Scene;

        void enter() override {
            background_ = &spawn<BackgroundLayer>(kSkyTop, kSkyBottom);
            card_ = &spawn<TitleCard>();
        }

        void update(float dt) override {
            Scene::update(dt);
            if (IsKeyPressed(KEY_ENTER) || input().pressed("jump")) {
                runtime().pushScene<GameScene>(0);
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                runtime().quit();
            }
        }

    private:
        BackgroundLayer* background_{nullptr};
        TitleCard* card_{nullptr};
    };

    namespace {
        [[nodiscard]] Vector2 tileCenter(const int x, const int y, const int tileW, const int tileH) {
            return Vector2{
                x * tileW + tileW * 0.5f,
                y * tileH + tileH * 0.5f
            };
        }

        [[nodiscard]] bool isSolidGid(const int gid) {
            return gid == 2 || gid == 4 || gid == 11;
        }

        const tson::Layer* findTileLayer(std::vector<tson::Layer>& layers) {
            for (auto& layer : layers) {
                if (layer.getType() == tson::LayerType::TileLayer) {
                    return &layer;
                }
                auto nested = findTileLayer(layer.getLayers());
                if (nested) {
                    return nested;
                }
            }
            return nullptr;
        }

        Rectangle circleBounds(const Vector2 center, const float radius) {
            return Rectangle{
                center.x - radius,
                center.y - radius,
                radius * 2.0f,
                radius * 2.0f
            };
        }
    }

    // ----- BackgroundLayer ----------------------------------------------------
    BackgroundLayer::BackgroundLayer(rlge::Scene& scene, const Color top, const Color bottom) :
        RenderEntity(scene),
        top_(top),
        bottom_(bottom) {}

    void BackgroundLayer::draw() {
        rq().submitBackground([top = top_, bottom = bottom_] {
            const float size = 10000.0f;
            DrawRectangleGradientV(
                static_cast<int>(-size),
                static_cast<int>(-size),
                static_cast<int>(size * 2.0f),
                static_cast<int>(size * 2.0f),
                top,
                bottom);
        });
    }

    // ----- CoinField ----------------------------------------------------------
    CoinField::CoinField(rlge::Scene& scene, GameScene& game, rlge::SpriteSheet& sheet) :
        RenderEntity(scene),
        game_(game),
        sheet_(sheet) {
        coinSrc_ = sheet_.tile(2, 2);
    }

    void CoinField::update(const float dt) {
        timer_ += dt;
    }

    void CoinField::draw() {
        auto& level = game_.currentLevel();
        Texture2D& tex = sheet_.texture();
        constexpr float scale = 1.8f;
        for (std::size_t i = 0; i < level.coins.size(); ++i) {
            if (i < level.coinCollected.size() && level.coinCollected[i])
                continue;
            const Vector2 pos = level.coins[i];
            const float bob = std::sin(timer_ * 4.0f + i * 0.45f) * 2.0f;
            Rectangle dest{
                pos.x,
                pos.y + bob,
                coinSrc_.width * scale,
                coinSrc_.height * scale
            };
            const Vector2 origin{dest.width * 0.5f, dest.height * 0.5f};
            rq().submitSprite(rlge::RenderLayer::World,
                              pos.y,
                              tex,
                              coinSrc_,
                              dest,
                              origin,
                              0.0f,
                              WHITE);
        }
    }

    // ----- GoalMarker ---------------------------------------------------------
    GoalMarker::GoalMarker(rlge::Scene& scene, GameScene& game, rlge::SpriteSheet& sheet) :
        RenderEntity(scene),
        game_(game),
        sheet_(sheet) {
        goalSrc_ = sheet_.tile(1, 0);
    }

    void GoalMarker::update(const float dt) {
        pulse_ += dt * 3.0f;
    }

    void GoalMarker::draw() {
        if (game_.runComplete())
            return;
        const Vector2 center = game_.exitCenter();
        const float scale = 2.5f + std::sin(pulse_) * 0.2f;
        Rectangle dest{
            center.x,
            center.y,
            goalSrc_.width * scale,
            goalSrc_.height * scale
        };
        const Vector2 origin{dest.width * 0.5f, dest.height * 0.5f};
        rq().submitSprite(rlge::RenderLayer::World,
                          center.y,
                          sheet_.texture(),
                          goalSrc_,
                          dest,
                          origin,
                          pulse_ * 10.0f,
                          ColorAlpha(RAYWHITE, 0.95f));
    }

    // ----- Hud ----------------------------------------------------------------
    Hud::Hud(rlge::Scene& scene, GameScene& game) :
        RenderEntity(scene),
        game_(game) {}

    void Hud::draw() {
        const int margin = 20;
        const auto levelIdx = game_.levelIndex() + 1;
        const auto levelCount = game_.levelCount();
        const std::string line1 = std::format("Level {}/{} — {}",
                                              levelIdx,
                                              levelCount,
                                              game_.currentLevel().def.name);
        const std::string line2 = std::format("Crystals: {}/{}",
                                              game_.coinsCollected(),
                                              std::max(1, game_.totalCoinsGoal()));
        const std::string line3 = std::format("Falls survived: {}", game_.deathCount());

        rq().submitUI([=] {
            DrawRectangle(margin - 10, margin - 10, 360, 120, Fade(BLACK, 0.35f));
            DrawText(line1.c_str(), margin, margin, 24, RAYWHITE);
            DrawText(line2.c_str(), margin, margin + 32, 22, Color{173, 216, 255, 255});
            DrawText(line3.c_str(), margin, margin + 60, 22, Color{255, 207, 160, 255});
            DrawText("A/D — move", margin, margin + 90, 20, GRAY);
            DrawText("SPACE — jump", margin + 160, margin + 90, 20, GRAY);
        });

        if (game_.isRespawning()) {
            rq().submitUI([] {
                DrawText("Respawning...", 20, kWindowHeight - 70, 28, WHITE);
            });
        }
        else if (game_.isTransitioning()) {
            rq().submitUI([] {
                DrawText("Checkpoint reached!", 20, kWindowHeight - 70, 28, WHITE);
            });
        }
        else if (game_.runComplete()) {
            rq().submitUI([] {
                const char* msg = "All levels cleared! Press Enter to return to the menu.";
                const int width = MeasureText(msg, 30);
                DrawRectangle((kWindowWidth - width) / 2 - 20,
                              kWindowHeight / 2 - 50,
                              width + 40,
                              100,
                              Fade(BLACK, 0.6f));
                DrawText(msg, (kWindowWidth - width) / 2, kWindowHeight / 2 - 10, 30, RAYWHITE);
            });
        }
    }

    // ----- Player -------------------------------------------------------------
    namespace {
        constexpr float kPlayerWidth = 10.0f;
        constexpr float kPlayerHeight = 14.0f;
        constexpr float kPlayerHalfWidth = kPlayerWidth * 0.5f;
        constexpr float kPlayerHalfHeight = kPlayerHeight * 0.5f;
        constexpr float kMoveSpeed = 75.0f;
        constexpr float kGroundAccel = 12.0f;
        constexpr float kAirAccel = 6.0f;
        constexpr float kGroundFriction = 14.0f;
        constexpr float kAirFriction = 2.0f;
        constexpr float kJumpVelocity = 175.0f;
        constexpr float kGravity = 420.0f;
        constexpr float kJumpCutMultiplier = 2.5f;
        constexpr float kMaxFallSpeed = 260.0f;
        constexpr float kCoyoteTime = 0.12f;
        constexpr float kJumpBuffer = 0.12f;
        constexpr int kExtraJumpCount = 1;
    }

    Player::Player(rlge::Scene& scene, GameScene& game, rlge::SpriteSheet& sheet) :
        RenderEntity(scene),
        game_(game) {
        transform_ = &add<rlge::Transform>();
        transform_->scale = {2.3f, 2.3f};
        sprite_ = &add<rlge::SheetSprite>(sheet, 2, 0);
    }

    void Player::respawn(const Vector2 pos) {
        if (!transform_)
            return;
        transform_->position = pos;
        velocity_ = {0, 0};
        onGround_ = false;
        coyoteTimer_ = 0.0f;
        jumpBuffer_ = 0.0f;
        extraJumps_ = kExtraJumpCount;
    }

    void Player::setControlEnabled(const bool enabled) {
        controlEnabled_ = enabled;
    }

    Vector2 Player::position() const {
        return transform_ ? transform_->position : Vector2{0, 0};
    }

    Rectangle Player::computeBounds(const Vector2 pos) const {
        return Rectangle{
            pos.x - kPlayerHalfWidth,
            pos.y - kPlayerHalfHeight,
            kPlayerWidth,
            kPlayerHeight
        };
    }

    Rectangle Player::bounds() const {
        return computeBounds(position());
    }

    void Player::integrateHorizontal(Rectangle& box, const float dt) {
        box.x += velocity_.x * dt;
        const int startY = static_cast<int>(std::floor(box.y / kTileSize));
        const int endY = static_cast<int>(std::floor((box.y + box.height - 1.0f) / kTileSize));

        if (velocity_.x > 0.0f) {
            const int tileX = static_cast<int>(std::floor((box.x + box.width) / kTileSize));
            for (int ty = startY; ty <= endY; ++ty) {
                if (game_.isSolid(tileX, ty)) {
                    const float tileLeft = tileX * kTileSize;
                    box.x = tileLeft - box.width - 0.01f;
                    velocity_.x = 0.0f;
                    break;
                }
            }
        }
        else if (velocity_.x < 0.0f) {
            const int tileX = static_cast<int>(std::floor(box.x / kTileSize));
            for (int ty = startY; ty <= endY; ++ty) {
                if (game_.isSolid(tileX, ty)) {
                    const float tileRight = (tileX + 1) * kTileSize;
                    box.x = tileRight + 0.01f;
                    velocity_.x = 0.0f;
                    break;
                }
            }
        }
    }

    void Player::integrateVertical(Rectangle& box, const float dt) {
        box.y += velocity_.y * dt;
        const int startX = static_cast<int>(std::floor(box.x / kTileSize));
        const int endX = static_cast<int>(std::floor((box.x + box.width - 1.0f) / kTileSize));

        onGround_ = false;
        if (velocity_.y > 0.0f) {
            const int tileY = static_cast<int>(std::floor((box.y + box.height) / kTileSize));
            for (int tx = startX; tx <= endX; ++tx) {
                if (game_.isSolid(tx, tileY)) {
                    const float tileTop = tileY * kTileSize;
                    box.y = tileTop - box.height - 0.01f;
                    velocity_.y = 0.0f;
                    onGround_ = true;
                    extraJumps_ = kExtraJumpCount;
                    break;
                }
            }
        }
        else if (velocity_.y < 0.0f) {
            const int tileY = static_cast<int>(std::floor(box.y / kTileSize));
            for (int tx = startX; tx <= endX; ++tx) {
                if (game_.isSolid(tx, tileY)) {
                    const float tileBottom = (tileY + 1) * kTileSize;
                    box.y = tileBottom + 0.01f;
                    velocity_.y = 0.0f;
                    break;
                }
            }
        }
    }

    void Player::update(const float dt) {
        RenderEntity::update(dt);
        if (!transform_)
            return;

        if (controlEnabled_) {
            float axis = 0.0f;
            if (input().down("left"))
                axis -= 1.0f;
            if (input().down("right"))
                axis += 1.0f;

            const float accel = onGround_ ? kGroundAccel : kAirAccel;
            const float target = axis * kMoveSpeed;
            velocity_.x += std::clamp(target - velocity_.x, -accel * dt * kMoveSpeed, accel * dt * kMoveSpeed);

            if (std::abs(axis) < 0.01f) {
                const float friction = (onGround_ ? kGroundFriction : kAirFriction) * dt * kMoveSpeed;
                if (std::abs(velocity_.x) <= friction)
                    velocity_.x = 0.0f;
                else
                    velocity_.x -= std::copysign(friction, velocity_.x);
            }

            if (axis > 0.05f)
                facingRight_ = true;
            else if (axis < -0.05f)
                facingRight_ = false;
        }
        else {
            velocity_.x = 0.0f;
        }

        if (transform_) {
            transform_->scale.x = std::abs(transform_->scale.x) * (facingRight_ ? 1.0f : -1.0f);
        }

        coyoteTimer_ = onGround_ ? kCoyoteTime : std::max(coyoteTimer_ - dt, -0.2f);
        jumpBuffer_ = std::max(0.0f, jumpBuffer_ - dt);
        if (controlEnabled_ && input().pressed("jump")) {
            jumpBuffer_ = kJumpBuffer;
        }

        if (jumpBuffer_ > 0.0f && (coyoteTimer_ > 0.0f || extraJumps_ > 0)) {
            velocity_.y = -kJumpVelocity;
            if (coyoteTimer_ <= 0.0f) {
                --extraJumps_;
            }
            jumpBuffer_ = 0.0f;
            coyoteTimer_ = 0.0f;
            onGround_ = false;
        }

        if (!input().down("jump") && velocity_.y < 0.0f) {
            velocity_.y += kGravity * (kJumpCutMultiplier - 1.0f) * dt;
        }

        velocity_.y = std::min(velocity_.y + kGravity * dt, kMaxFallSpeed);

        Rectangle box = bounds();
        integrateHorizontal(box, dt);
        integrateVertical(box, dt);

        transform_->position = {
            box.x + kPlayerHalfWidth,
            box.y + kPlayerHalfHeight
        };

        const auto& level = game_.currentLevel();
        if (box.y > level.height * level.tileHeight + 64.0f) {
            game_.handlePlayerDeath();
            return;
        }

        if (game_.collectCoins(box) > 0) {
            // Add a tiny bounce when grabbing coins.
            velocity_.y -= 15.0f;
        }

        if (game_.checkHazard(box)) {
            game_.handlePlayerDeath();
            return;
        }

        if (game_.checkGoal(box)) {
            game_.completeLevel();
        }
    }

    // ----- TitleCard ----------------------------------------------------------
    TitleCard::TitleCard(rlge::Scene& scene) :
        RenderEntity(scene) {}

    void TitleCard::update(const float dt) {
        timer_ += dt;
    }

    void TitleCard::draw() {
        rq().submitUI([timer = timer_] {
            const char* title = "RLGE PLATFORMER";
            const char* subtitle = "A tiny two-level adventure";
            const int titleSize = 64;
            const int subtitleSize = 28;
            const int titleWidth = MeasureText(title, titleSize);
            const int subtitleWidth = MeasureText(subtitle, subtitleSize);
            DrawText(title,
                     (kWindowWidth - titleWidth) / 2,
                     kWindowHeight / 3,
                     titleSize,
                     RAYWHITE);
            DrawText(subtitle,
                     (kWindowWidth - subtitleWidth) / 2,
                     kWindowHeight / 3 + 80,
                     subtitleSize,
                     Color{173, 216, 255, 255});

            const bool blink = std::fmod(timer, 1.0f) < 0.5f;
            if (blink) {
                const char* prompt = "Press Enter or Space to play";
                const int promptWidth = MeasureText(prompt, 24);
                DrawText(prompt,
                         (kWindowWidth - promptWidth) / 2,
                         kWindowHeight / 3 + 160,
                         24,
                         WHITE);
            }
        });
    }

    // ----- GameScene ----------------------------------------------------------
    GameScene::GameScene(rlge::Runtime& runtime, int startingLevel) :
        Scene(runtime),
        requestedLevel_(startingLevel) {}

    void GameScene::enter() {
        Scene::enter();
        background_ = &spawn<BackgroundLayer>(kSkyTop, kSkyBottom);
        tileset_ = &assets().loadTexture("platformer_sheet", "../examples/platformer/assets/sprites/spritesheet.png");
        spriteSheet_ = std::make_unique<rlge::SpriteSheet>(*tileset_, static_cast<int>(kTileSize), static_cast<int>(kTileSize));
        coinField_ = &spawn<CoinField>(*this, *spriteSheet_);
        goal_ = &spawn<GoalMarker>(*this, *spriteSheet_);
        player_ = &spawn<Player>( *this, *spriteSheet_);
        hud_ = &spawn<Hud>( *this);

        buildLevels();
        activateLevel(std::clamp(requestedLevel_, 0, static_cast<int>(levels_.size()) - 1));

        camera().setOffset(Vector2{kWindowWidth / 2.0f, kWindowHeight / 2.0f});
        camera().setZoom(kCameraZoom);
    }

    void GameScene::update(const float dt) {
        Scene::update(dt);
        time_ += dt;

        if (runComplete_) {
            if (IsKeyPressed(KEY_ENTER) || input().pressed("jump")) {
                runtime().popScene();
            }
            return;
        }

        if (IsKeyPressed(KEY_ESCAPE)) {
            runtime().popScene();
            return;
        }

        if (IsKeyPressed(KEY_R)) {
            restartLevel();
        }

        if (respawning_) {
            respawnTimer_ -= dt;
            if (respawnTimer_ <= 0.0f) {
                respawning_ = false;
                player_->respawn(currentLevel().spawn);
                player_->setControlEnabled(true);
            }
        }

        if (transitioning_) {
            transitionTimer_ -= dt;
            if (transitionTimer_ <= 0.0f) {
                transitioning_ = false;
                advanceLevel();
            }
        }

        updateCamera(dt);
    }

    rlge::SpriteSheet& GameScene::sheet() {
        return *spriteSheet_;
    }

    LevelState& GameScene::currentLevel() {
        return levels_[currentLevel_];
    }

    const LevelState& GameScene::currentLevel() const {
        return levels_[currentLevel_];
    }

    Vector2 GameScene::playerPosition() const {
        return player_ ? player_->position() : currentLevel().spawn;
    }

    bool GameScene::isSolid(const int tx, const int ty) const {
        const auto& level = currentLevel();
        if (tx < 0 || ty < 0 || tx >= level.width || ty >= level.height)
            return true;
        const auto idx = static_cast<std::size_t>(ty) * level.width + tx;
        return idx < level.solidMask.size() && level.solidMask[idx] != 0;
    }

    bool GameScene::checkHazard(const Rectangle& rect) const {
        const auto& level = currentLevel();
        for (const auto& hazard : level.hazards) {
            if (CheckCollisionRecs(rect, hazard))
                return true;
        }
        return rect.y > level.height * level.tileHeight + 16.0f;
    }

    bool GameScene::checkGoal(const Rectangle& rect) const {
        return CheckCollisionRecs(rect, currentLevel().exitRect);
    }

    int GameScene::collectCoins(const Rectangle& rect) {
        auto& level = currentLevel();
        int collected = 0;
        for (std::size_t i = 0; i < level.coins.size(); ++i) {
            if (i < level.coinCollected.size() && level.coinCollected[i])
                continue;
            const Rectangle coinRect = circleBounds(level.coins[i], 3.5f);
            if (CheckCollisionRecs(rect, coinRect)) {
                if (i >= level.coinCollected.size())
                    level.coinCollected.resize(level.coins.size(), 0);
                level.coinCollected[i] = 1;
                ++collected;
            }
        }
        totalCoinsCollected_ += collected;
        return collected;
    }

    void GameScene::handlePlayerDeath() {
        if (respawning_ || transitioning_)
            return;
        ++deaths_;
        respawning_ = true;
        respawnTimer_ = 0.9f;
        if (player_) {
            player_->setControlEnabled(false);
        }
    }

    void GameScene::completeLevel() {
        if (transitioning_ || respawning_ || runComplete_)
            return;
        transitioning_ = true;
        transitionTimer_ = 0.8f;
        if (player_) {
            player_->setControlEnabled(false);
        }
    }

    void GameScene::restartLevel() {
        auto& level = currentLevel();
        totalCoinsCollected_ -= level.resetCoins();
        respawning_ = false;
        transitioning_ = false;
        runComplete_ = false;
        if (player_) {
            player_->respawn(level.spawn);
            player_->setControlEnabled(true);
        }
    }

    bool GameScene::canControlPlayer() const {
        return !respawning_ && !transitioning_ && !runComplete_;
    }

    void GameScene::buildLevels() {
        levels_.clear();
        totalCoins_ = 0;
        for (const auto& def : levelDefs_) {
            levels_.push_back(loadLevel(def));
            totalCoins_ += levels_.back().totalCoins();
        }
        for (std::size_t i = 0; i < levels_.size(); ++i) {
            if (auto* tr = levels_[i].tilemap->get<rlge::Transform>()) {
                tr->position = Vector2{kLevelHideOffset + static_cast<float>(i) * 2000.0f, 0.0f};
            }
        }
    }

    LevelState GameScene::loadLevel(const LevelDef& def) {
        tson::Tileson parser;
        auto map = parser.parse(def.path);
        if (!map || map->getStatus() != tson::ParseStatus::OK) {
            throw std::runtime_error("Failed to load level: " + def.path.string());
        }

        const auto* tileLayer = findTileLayer(map->getLayers());
        if (!tileLayer) {
            throw std::runtime_error("Level has no tile layer");
        }

        if (map->getOrientation() != "orthogonal") {
            throw std::runtime_error("Only orthogonal maps are supported");
        }

        const auto& tileset = map->getTilesets().front();
        const int mapW = tileLayer->getSize().x;
        const int mapH = tileLayer->getSize().y;
        const int tileW = map->getTileSize().x;
        const int tileH = map->getTileSize().y;
        const int backgroundIndex = kGidBackground - tileset.getFirstgid();

        std::vector<rlge::Tilemap::TileCell> cells(static_cast<std::size_t>(mapW) * mapH,
                                                   rlge::Tilemap::TileCell{backgroundIndex, 0});
        std::vector<std::uint8_t> solidMask(static_cast<std::size_t>(mapW) * mapH, 0);

        LevelState level;
        level.def = def;
        level.width = mapW;
        level.height = mapH;
        level.tileWidth = tileW;
        level.tileHeight = tileH;

        for (const auto& entry : tileLayer->getTileData()) {
            const auto [tileX, tileY] = entry.first;
            const auto tile = entry.second;
            if (!tile || tileX < 0 || tileY < 0 || tileX >= mapW || tileY >= mapH)
                continue;

            const int gid = static_cast<int>(tile->getGid());
            const std::size_t idx = static_cast<std::size_t>(tileY) * mapW + tileX;

            bool skip = false;
            if (gid == kGidSpawn || gid == kGidSpawnAlt) {
                level.spawn = tileCenter(tileX, tileY, tileW, tileH);
                skip = true;
            }
            else if (gid == kGidExit) {
                level.exitRect = Rectangle{
                    static_cast<float>(tileX * tileW),
                    static_cast<float>(tileY * tileH),
                    static_cast<float>(tileW),
                    static_cast<float>(tileH)
                };
                skip = true;
            }
            else if (gid == kGidCoin) {
                level.coins.push_back(tileCenter(tileX, tileY, tileW, tileH));
                level.coinCollected.push_back(0);
                skip = true;
            }
            else if (gid == kGidHazard) {
                level.hazards.push_back(Rectangle{
                    static_cast<float>(tileX * tileW),
                    static_cast<float>(tileY * tileH),
                    static_cast<float>(tileW),
                    static_cast<float>(tileH)
                });
            }

            if (idx < solidMask.size()) {
                solidMask[idx] = isSolidGid(gid) ? 1 : 0;
            }

            if (skip) {
                cells[idx].index = backgroundIndex;
                cells[idx].flipFlags = 0;
            }
            else if (gid == 0) {
                cells[idx].index = -1;
                cells[idx].flipFlags = 0;
            }
            else {
                cells[idx].index = gid - tileset.getFirstgid();
                cells[idx].flipFlags = static_cast<std::uint32_t>(tile->getFlipFlags());
            }
        }

        if (level.spawn.x == 0.0f && level.spawn.y == 0.0f) {
            level.spawn = tileCenter(1, mapH - 2, tileW, tileH);
        }

        if (level.exitRect.width == 0.0f) {
            level.exitRect = Rectangle{
                static_cast<float>((mapW - 2) * tileW),
                static_cast<float>((mapH - 3) * tileH),
                static_cast<float>(tileW),
                static_cast<float>(tileH)
            };
        }

        int margin = tileset.getMargin();
        int spacing = tileset.getSpacing();
        int columns = tileset.getColumns();
        if (columns <= 0) {
            const auto imageSize = tileset.getImageSize();
            const int pitch = tileset.getTileSize().x + spacing;
            if (pitch > 0) {
                columns = std::max(1, (imageSize.x - margin * 2 + spacing) / pitch);
            }
        }

        auto& tilemap = spawn<rlge::Tilemap>(*tileset_, tileW, tileH, mapW, mapH, std::move(cells), margin, spacing, columns);
        if (auto* tr = tilemap.get<rlge::Transform>()) {
            tr->position = Vector2{kLevelHideOffset, 0.0f};
        }

        level.tilemap = &tilemap;
        level.solidMask = std::move(solidMask);
        return level;
    }

    void GameScene::activateLevel(const int index) {
        currentLevel_ = std::clamp(index, 0, static_cast<int>(levels_.size()) - 1);
        for (std::size_t i = 0; i < levels_.size(); ++i) {
            if (auto* tr = levels_[i].tilemap->get<rlge::Transform>()) {
                tr->position = i == static_cast<std::size_t>(currentLevel_)
                                   ? Vector2{0.0f, 0.0f}
                                   : Vector2{kLevelHideOffset + static_cast<float>(i) * 2000.0f, 0.0f};
            }
        }
        if (player_) {
            player_->respawn(currentLevel().spawn);
            player_->setControlEnabled(true);
        }
    }

    void GameScene::advanceLevel() {
        if (currentLevel_ + 1 >= static_cast<int>(levels_.size())) {
            runComplete_ = true;
            if (player_) {
                player_->setControlEnabled(false);
            }
            return;
        }
        activateLevel(currentLevel_ + 1);
    }

    void GameScene::updateCamera(float) {
        const auto size = runtime().window().size();
        const float halfWidth = (size.x / camera().zoom()) * 0.5f;
        const float halfHeight = (size.y / camera().zoom()) * 0.5f;
        const float levelWidth = currentLevel().width * currentLevel().tileWidth;
        const float levelHeight = currentLevel().height * currentLevel().tileHeight;

        Vector2 target = playerPosition();
        if (levelWidth > halfWidth * 2.0f) {
            target.x = std::clamp(target.x, halfWidth, levelWidth - halfWidth);
        }
        else {
            target.x = levelWidth * 0.5f;
        }

        if (levelHeight > halfHeight * 2.0f) {
            target.y = std::clamp(target.y, halfHeight, levelHeight - halfHeight);
        }
        else {
            target.y = levelHeight * 0.5f;
        }

        camera().setTarget(target);
    }
}

int main() {
    const rlge::WindowConfig cfg{
        .width = demo::kWindowWidth,
        .height = demo::kWindowHeight,
        .fps = 144,
        .title = "RLGE Platformer"
    };
    rlge::Runtime runtime(cfg);

    runtime.input().bind("left", KEY_A);
    runtime.input().bind("right", KEY_D);
    runtime.input().bind("jump", KEY_SPACE);

    runtime.pushScene<demo::TitleScene>();
    runtime.run();
    return 0;
}
