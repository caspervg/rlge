#include "snake_game.hpp"

#include <random>
#include "events.hpp"

namespace {
    bool isOpposite(const snake::Direction a, const snake::Direction b) {
        using snake::Direction;
        return (a == Direction::Left && b == Direction::Right) ||
               (a == Direction::Right && b == Direction::Left) ||
               (a == Direction::Up && b == Direction::Down) ||
               (a == Direction::Down && b == Direction::Up);
    }
}

namespace snake {

    Game::Game(const Config& cfg, rlge::EventBus* bus)
        : cfg_(cfg)
          , tilePixels_(cfg.pixelsPerTile * cfg.magnification)
          , screenWidth_(cfg.tilesX * cfg.pixelsPerTile * cfg.magnification)
          , screenHeight_(cfg.tilesY * cfg.pixelsPerTile * cfg.magnification)
          , appleX_(cfg.tilesX / 2 + 2)
          , appleY_(cfg.tilesY / 2)
          , dir_(Direction::Right)
          , moveInterval_(1 / cfg.movesPerSecond)
          , rng_(std::random_device{}())
          , appleXRng_(1, cfg.tilesX - 2)
          , appleYRng_(1, cfg.tilesY - 2)
          , bus_(bus) {
        const int cx = cfg.tilesX / 2;
        const int cy = cfg.tilesY / 2;
        body_.push_back(Cell{cx, cy});
        body_.push_back(Cell{cx - 1, cy});
        body_.push_back(Cell{cx - 2, cy});

        spawnApple();
    }

    void Game::setDirection(const Direction dir) {
        const Direction lastDir = directionQueue_.empty() ? dir_ : directionQueue_.back();
        if (dir == lastDir || isOpposite(lastDir, dir)) {
            return;
        }
        directionQueue_.push_back(dir);
    }

    void Game::update(float dt) {
        moveAccum_ += dt;
        while (moveAccum_ >= moveInterval_) {
            moveAccum_ -= moveInterval_;
            step();
        }
    }

    Vector2 Game::headWorldPos() const {
        if (body_.empty())
            return tileCenter(cfg_.tilesX / 2, cfg_.tilesY / 2);
        return tileCenter(body_.front().x, body_.front().y);
    }

    Vector2 Game::appleWorldPos() const {
        return tileCenter(appleX_, appleY_);
    }

    void Game::step() {
        if (body_.empty())
            return;

        if (!directionQueue_.empty()) {
            dir_ = directionQueue_.front();
            directionQueue_.pop_front();
        }

        int nextX = body_.front().x;
        int nextY = body_.front().y;
        switch (dir_) {
        case Direction::Left:  --nextX; break;
        case Direction::Right: ++nextX; break;
        case Direction::Up:    --nextY; break;
        case Direction::Down:  ++nextY; break;
        }

        // Simple wall collision: borders are walls.
        if (nextX <= 0 || nextX >= cfg_.tilesX - 1 ||
            nextY <= 0 || nextY >= cfg_.tilesY - 1) {
            if (bus_) {
                bus_->enqueue(SnakeDied{});
            }
            return;
        }

        // Self collision: check against body except the current tail,
        // which will move forward if we don't grow this step.
        const int lastIndex = static_cast<int>(body_.size()) - 1;
        for (int i = 0; i < lastIndex; ++i) {
            if (body_[i].x == nextX && body_[i].y == nextY) {
                if (bus_) {
                    bus_->enqueue(SnakeDied{});
                }
                return;
            }
        }

        const bool ateApple = (nextX == appleX_ && nextY == appleY_);

        // Insert new head at the front.
        body_.insert(body_.begin(), Cell{nextX, nextY});

        if (ateApple) {
            ++score_;
            if (bus_) {
                bus_->enqueue(AppleEaten{1});
            }
            spawnApple();
            moveInterval_ *= 0.97f; // Speed up a little each time
        } else {
            // Move forward without growing: drop tail.
            body_.pop_back();
        }
    }

    void Game::spawnApple() {
        while (true) {
            int ax = appleXRng_(rng_);
            int ay = appleYRng_(rng_);

            bool onSnake = false;
            for (const auto& c : body_) {
                if (c.x == ax && c.y == ay) {
                    onSnake = true;
                    break;
                }
            }
            if (!onSnake) {
                appleX_ = ax;
                appleY_ = ay;
                break;
            }
        }
    }

    Vector2 Game::tileCenter(const int gx, const int gy) const {
        return {
            gx * static_cast<float>(tilePixels_) - screenWidth_ / 2.0f + tilePixels_ / 2.0f,
            gy * static_cast<float>(tilePixels_) - screenHeight_ / 2.0f + tilePixels_ / 2.0f
        };
    }

} // namespace snake
