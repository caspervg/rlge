#pragma once

#include <memory>

#include "debug.hpp"
#include "render_entity.hpp"
#include "snake_game.hpp"
#include "sprite_sheet.hpp"

struct ImGuiContext; // forward-declare to avoid including imgui.h here

namespace snake {

    // Simple FPS overlay.
    class FpsCounter final : public rlge::RenderEntity {
    public:
        explicit FpsCounter(rlge::Scene& scene)
            : RenderEntity(scene) {}

        void draw() override;
    };

    // Background grid using rlgl.
    class Background final : public rlge::RenderEntity {
    public:
        explicit Background(rlge::Scene& scene)
            : RenderEntity(scene) {}

        void draw() override;

    private:
        bool visible_ = true;

        friend class GameScene;
    };

    // View entity for the snake head; reads position from Game.
    class SnakeHead final : public rlge::RenderEntity {
    public:
        SnakeHead(rlge::Scene& scene, Game& game, rlge::SpriteSheet& sheet);

        void update(float dt) override;

    private:
        Game& game_;
        rlge::SheetSprite* sprite_{nullptr};
    };

    // View entity for the snake body segments (excluding head).
    class SnakeBody final : public rlge::RenderEntity {
    public:
        SnakeBody(rlge::Scene& scene, Game& game, rlge::SpriteSheet& sheet);

        void draw() override;

    private:
        Game& game_;
        rlge::SpriteSheet& sheet_;
    };

    class BorderTile final : public rlge::RenderEntity {
    public:
        BorderTile(rlge::Scene& scene, Game& game, rlge::SpriteSheet& sheet, int xg, int yg);
        void draw() override;

    private:
        rlge::SpriteSheet& sheet_;
        Game& game_;
        int spriteCol_{0};
        int rotation_{0};
        int xg_, yg_;
    };

    // Renders border tiles around the edge of the board.
    class BorderTiles final : public rlge::RenderEntity {
    public:
        BorderTiles(rlge::Scene& scene, Game& game, rlge::SpriteSheet& sheet);

        void draw() override;

    private:
        std::vector<std::unique_ptr<BorderTile>> tiles_;
    };

    // View entity for the apple; reads position from Game.
    class AppleSprite final : public rlge::RenderEntity {
    public:
        AppleSprite(rlge::Scene& scene, Game& game, rlge::SpriteSheet& sheet);

        void update(float dt) override;

        void changeSprite() const;
    private:
        [[nodiscard]] int randomSpriteRow() const;

        Game& game_;
        rlge::SheetSprite* sprite_{nullptr};
        std::vector<int> sheetSpriteRows_{0, 1, 3, 4};
    };

    class Scoreboard final : public rlge::RenderEntity {
    public:
        explicit Scoreboard(rlge::Scene& scene, int& score) :
            RenderEntity(scene), score_(score) {}

        void draw() override;

        void toggleVisibility() { visible_ = !visible_; }
    private:
        int& score_;
        bool visible_ = true;
    };

    class GameScene final : public rlge::Scene, public rlge::HasDebugOverlay {
    public:
        explicit GameScene(rlge::Runtime& r);
        ~GameScene() override;

        void enter() override;
        void update(float dt) override;
        void exit() override;
        void debugOverlay() override;

    private:
        Game game_;

        Background* bg_{nullptr};
        Scoreboard* scoreboard_{nullptr};
        SnakeHead* snake_{nullptr};
        SnakeBody* snakeBody_{nullptr};
        BorderTiles* borders_{nullptr};
        AppleSprite* apple_{nullptr};
        FpsCounter* fps_{nullptr};

        rlge::Camera camera_;

        std::unique_ptr<rlge::SpriteSheet> spriteSheet_;
        rlge::EventBus::SubscriptionId appleSubId_{0};
        rlge::EventBus::SubscriptionId diedSubId_{0};
        int score_ = 0;
    };

} // namespace snake
