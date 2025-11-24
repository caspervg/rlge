#include <print>

#include "box_collider.hpp"
#include "breakout_game.hpp"
#include "circle_collider.hpp"
#include "physics_body.hpp"
#include "render_entity.hpp"
#include "runtime.hpp"
#include "scene.hpp"
#include "transformer.hpp"

namespace breakout {
    using namespace rlge;
    using CLM = ColliderLayerMask;
    Config g_cfg;

    class Wall final : public Entity {
    public:
        explicit Wall(Scene& s, float x, float y, float w, float h) :
            Entity(s) {
            auto& tr = add<rlge::Transform>();
            tr.position = {x + w / 2.0f, y + h / 2.0f};

            add<BoxCollider>(
                scene().collisions(),
                ColliderType::Kinematic,
                ColliderLayerMask::LAYER_WORLD,
                ColliderLayerMask::LAYER_BULLET,
                Rectangle{-w / 2.0f, -h / 2.0f, w, h},
                false);
        }
    };

    class Paddle final : public RenderEntity {
    public:
        explicit Paddle(Scene& s) :
            RenderEntity(s) {
            auto& tr = add<rlge::Transform>();
            tr.position = {g_cfg.width / 2.0f, g_cfg.height - 20.0f - g_cfg.paddleHeight / 2.0f};

            PhysicsBodyConfig conf = {
                .mass = 1.0f,
                .velocity = {0.f, 0.f},
                .type = BodyType::Kinematic
            };
            physics_ = &add<PhysicsBody>(conf);

            coll_ = &add<BoxCollider>(
                scene().collisions(),
                ColliderType::Kinematic,
                ColliderLayerMask::LAYER_PLAYER,
                ColliderLayerMask::LAYER_BULLET,
                Rectangle{-g_cfg.paddleWidth / 2.0f, -g_cfg.paddleHeight / 2.0f,
                          g_cfg.paddleWidth * 1.0f, g_cfg.paddleHeight * 1.0f},
                false
                );
        }

        void update(float dt) override {
            RenderEntity::update(dt);

            auto moveSpeed = 400.0f; // Adjust to your preference
            auto* tr = get<rlge::Transform>();
            if (!tr)
                return;

            if (input().down("left")) {
                tr->position.x -= moveSpeed * dt;
            }
            if (input().down("right")) {
                tr->position.x += moveSpeed * dt;
            }

            const float halfWidth = g_cfg.paddleWidth / 2.0f;
            if (tr->position.x < halfWidth) {
                tr->position.x = halfWidth;
            }
            if (tr->position.x > g_cfg.width - halfWidth) {
                tr->position.x = g_cfg.width - halfWidth;
            }
        }

        void draw() override {
            RenderEntity::draw();

            rq().submitWorld([this] {
                const auto* tr = get<rlge::Transform>();
                if (!tr)
                    return;
                DrawRectangle(
                    static_cast<int>(tr->position.x - g_cfg.paddleWidth / 2.0f),
                    static_cast<int>(tr->position.y - g_cfg.paddleHeight / 2.0f),
                    g_cfg.paddleWidth,
                    g_cfg.paddleHeight,
                    BLUE);
            });
        }

    private:
        PhysicsBody* physics_{nullptr};
        BoxCollider* coll_{nullptr};
    };

    class Brick final : public RenderEntity {
    public:
        Brick(Scene& s, float x, float y) :
            RenderEntity(s) {
            auto& tr = add<rlge::Transform>();
            tr.position = {x, y};

            coll_ = &add<BoxCollider>(
                scene().collisions(),
                ColliderType::Solid,
                ColliderLayerMask::LAYER_WORLD,
                ColliderLayerMask::LAYER_BULLET,
                Rectangle{-g_cfg.brickWidth / 2.0f, -g_cfg.brickHeight / 2.0f, g_cfg.brickWidth * 1.0f,
                          g_cfg.brickHeight * 1.0f},
                false);
        }

        void onCollision(const CollisionEvent& event) {
            coll_->unregisterCollider();
            if (event.state == CollisionState::Enter && alive_) {
                alive_ = false;
                scene().gameEvents().enqueue(BrickDestroyed{10});
            }
        }

        void draw() override {
            RenderEntity::draw();
            if (!alive_)
                return;

            rq().submitWorld([this] {
                const auto* tr = get<rlge::Transform>();
                if (!tr)
                    return;
                DrawRectangle(
                    static_cast<int>(tr->position.x - g_cfg.brickWidth / 2.0f),
                    static_cast<int>(tr->position.y - g_cfg.brickHeight / 2.0f),
                    g_cfg.brickWidth,
                    g_cfg.brickHeight,
                    LIME);
            });
        }

    private:
        BoxCollider* coll_{nullptr};
        bool alive_ = true;
    };

    class Ball final : public RenderEntity {
    public:
        explicit Ball(Scene& s) :
            RenderEntity(s) {
            auto& tr = add<rlge::Transform>();
            tr.position = {g_cfg.width / 2.0f, g_cfg.height / 2.0f};

            PhysicsBodyConfig conf = {
                .mass = 1.0f,
                .velocity = {250.0f, -250.0f},
                .gravity = {0.0f, 0.0f},
                .type = BodyType::Dynamic
            };
            physics_ = &add<PhysicsBody>(conf);

            col_ = &add<CircleCollider>(
                scene().collisions(),
                ColliderType::Solid,
                CLM::LAYER_BULLET,
                toLayerMask(CLM::LAYER_PLAYER | CLM::LAYER_WORLD),
                Vector2{0.0f, 0.0f},
                static_cast<float>(g_cfg.ballRadius),
                false
                );
        }

        void update(const float dt) override {
            RenderEntity::update(dt);
            auto* tr = get<rlge::Transform>();
            if (!tr)
                return;

            // The ball fell off
            if (tr->position.y > g_cfg.height) {
                scene().gameEvents().enqueue(BallLost{});
            }
        }

        void draw() override {
            RenderEntity::draw();
            rq().submitWorld([this] {
                const auto* tr = get<rlge::Transform>();
                if (!tr)
                    return;
                DrawCircleV(tr->position, g_cfg.ballRadius, YELLOW);
            });
        }

    private:
        PhysicsBody* physics_{nullptr};
        CircleCollider* col_{nullptr};
    };

    class BreakoutScene final : public Scene {
    public:
        explicit BreakoutScene(Runtime& r) :
            Scene(r) {}

        void enter() override {
            camera_ = rlge::Camera();
            setSingleView(camera_);

            paddle_ = &spawn<Paddle>();
            ball_ = &spawn<Ball>();

            // Spawn bricks
            for (auto row = 0; row < g_cfg.brickRows; ++row) {
                for (auto col = 0; col < g_cfg.brickColumns; ++col) {
                    constexpr auto startY = 50.0f;
                    constexpr auto startX = 50.0f;
                    const float x = startX + col * (g_cfg.brickWidth + g_cfg.brickMargin);
                    const float y = startY + row * (g_cfg.brickHeight + g_cfg.brickMargin);
                    bricks_.push_back(&spawn<Brick>(x, y));
                }
            }

            // Spawn walls
            constexpr auto wallThickness = 20.0f;
            spawn<Wall>(0, -wallThickness, g_cfg.width, wallThickness);
            spawn<Wall>(-wallThickness, 0, wallThickness, g_cfg.height);
            spawn<Wall>(g_cfg.width, 0, wallThickness, g_cfg.height);

            // Register game-specific collision response
            collisionResponses().addHandler([](Entity& entity, const CollisionEvent& event) {
                if (auto* brick = dynamic_cast<Brick*>(&entity)) {
                    brick->onCollision(event);
                }
            });

            // Subscribe to game events
            gameEvents().subscribe<BrickDestroyed>([this](const BrickDestroyed& e) {
                score_ += e.points;
            });

            gameEvents().subscribe<BallLost>([this](const BallLost&) {
                lives_--;
                if (lives_ <= 0) {
                    gameEvents().enqueue(GameWon{});
                }
                else {
                    //resetBall();
                }
            });
        }

    private:
        rlge::Camera camera_;
        int lives_ = 3;
        int score_ = 0;
        Paddle* paddle_{nullptr};
        Ball* ball_{nullptr};
        std::vector<Brick*> bricks_;
    };
}
