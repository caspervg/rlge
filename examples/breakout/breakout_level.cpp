#include "breakout_level.hpp"

#include <format>
#include <optional>
#include <set>

#include "breakout_entities.hpp"
#include "include/cpptoml.h"

namespace breakout {

    LevelManager::LevelManager() :
        LevelManager("../examples/breakout/assets/levels.toml") {}

    LevelManager::LevelManager(std::string_view levelFile) {
        const auto levels = cpptoml::parse_file(levelFile.data());
        const auto levelsArr = levels->get_table_array("levels");
        if (!levelsArr) {
            throw std::runtime_error{"No levels found in level file"};
        }

        for (const auto& levelTab : levelsArr->get()) {
            Level newLevel = {};
            newLevel.ballRadius = levelTab->get_as<double>("ballRadius")
                                          .value_or(newLevel.ballRadius);
            newLevel.ballVelocityMaximum = levelTab->get_as<double>("ballVelocityMaximum")
                                                   .value_or(newLevel.ballVelocityMaximum);
            newLevel.ballVelocityMultiplier = levelTab->get_as<double>("ballVelocityMultiplier")
                                                      .value_or(newLevel.ballVelocityMultiplier);
            newLevel.lives = levelTab->get_as<int>("lives")
                                     .value_or(newLevel.lives);
            newLevel.brickRows = levelTab->get_as<int>("brickRows")
                                         .value_or(newLevel.brickRows);
            newLevel.brickColumns = levelTab->get_as<int>("brickColumns")
                                            .value_or(newLevel.brickColumns);
            newLevel.name = levelTab->get_as<std::string>("name")
                                    .value_or(newLevel.name);
            newLevel.paddleSpeed = levelTab->get_as<double>("paddleSpeed")
                                           .value_or(newLevel.paddleSpeed);

            // Parse velocity
            auto const velocityStart = levelTab->get_array_of<double>("ballVelocityStart");
            if (velocityStart && velocityStart->size() == 2) {
                newLevel.ballVelocityStart = Vector2(velocityStart->at(0), velocityStart->at(1));
            }

            // Parse bricks
            const auto bricksArr = levelTab->get_table_array("bricks");
            if (!bricksArr) {
                newLevel.bricks = generateBricks_(newLevel.brickRows, newLevel.brickColumns);
            }
            else {
                newLevel.bricks = parseBricks_(newLevel.brickRows, newLevel.brickColumns, bricksArr);
            }

            levels_.push_back(std::make_unique<Level>(newLevel));
        }

        currentLevelIndex_ = 0;
    }

    const Level* LevelManager::currentLevel() const { return getLevel(currentLevelIndex_); }

    size_t LevelManager::currentLevelIndex() const { return currentLevelIndex_; }

    const Level* LevelManager::getLevel(const size_t index) const {
        if (levels_.size() <= index || index < 0)
            return nullptr;
        return levels_[index].get();
    }

    bool LevelManager::nextLevel() {
        if (currentLevelIndex_ + 1 >= levels_.size())
            return false;
        currentLevelIndex_++;
        return true;
    }

    size_t LevelManager::numLevels() const { return levels_.size(); }

    void LevelManager::reset() { currentLevelIndex_ = 0; }

    // ReSharper disable once CppDFAConstantFunctionResult
    BrickType LevelManager::brickTypeFromString_(const std::string& str) {
        if (str == "standard")
            return Standard;
        return Standard;
    }

    std::vector<BrickConfig> LevelManager::generateBricks_(const int rows, const int columns) {
        std::vector<BrickConfig> bricks;
        bricks.reserve(static_cast<size_t>(rows * columns));
        for (auto r = 0; r < rows; r++) {
            for (auto c = 0; c < columns; c++) {
                bricks.push_back(BrickConfig{c, r, 1, LIME, Standard});
            }
        }
        return bricks;
    }

    namespace {
        std::optional<PowerUpType> powerUpFromString(const std::string& str) {
            if (str == "WidePaddle") return PowerUpType::WidePaddle;
            if (str == "NarrowPaddle") return PowerUpType::NarrowPaddle;
            if (str == "LaserPaddle") return PowerUpType::LaserPaddle;
            if (str == "StickyPaddle") return PowerUpType::StickyPaddle;
            if (str == "MultiBall") return PowerUpType::MultiBall;
            if (str == "FireBall") return PowerUpType::FireBall;
            if (str == "SlowBall") return PowerUpType::SlowBall;
            if (str == "FastBall") return PowerUpType::FastBall;
            if (str == "ExtraLife") return PowerUpType::ExtraLife;
            if (str == "ScoreMultiplier") return PowerUpType::ScoreMultiplier;
            if (str == "SafetyNet") return PowerUpType::SafetyNet;
            return std::nullopt;
        }
    }

    std::vector<BrickConfig> LevelManager::parseBricks_(
        const int rows,
        const int columns,
        const std::shared_ptr<cpptoml::table_array>& bricksArr
        ) {
        std::vector<BrickConfig> bricks{};
        bricks.reserve(bricksArr->get().size());
        std::set<std::pair<int, int>> occupied;

        for (auto i = 0; i < bricksArr->get().size(); i++) {
            const auto brickTab = bricksArr->get()[i]->as_table();
            if (!brickTab)
                continue;

            BrickConfig brick;

            // Parse position
            auto posArr = brickTab->get_array_of<int64_t>("position");
            if (!posArr || posArr->size() != 2)
                continue;

            if (posArr->at(0) < 0 || posArr->at(0) >= rows) {
                throw std::runtime_error{std::format(
                    "Invalid brick position {} with brickColumns {}",
                    posArr->at(0), rows)
                };
            }
            if (posArr->at(1) < 0 || posArr->at(1) >= columns) {
                throw std::runtime_error{std::format(
                    "Invalid brick position {} with brickRows {}",
                    posArr->at(1), columns)
                };
            }

            brick.y = static_cast<int>(posArr->at(0));
            brick.x = static_cast<int>(posArr->at(1));

            // Avoid duplicates
            if (occupied.contains({brick.x, brick.y})) {
                continue;
            }
            occupied.insert({brick.x, brick.y});

            // Parse type
            auto typeVal = brickTab->get_as<std::string>("type").value_or("standard");
            brick.type = brickTypeFromString_(typeVal);

            // Parse hitpoints
            brick.hitPoints = brickTab->get_as<int>("hitPoints").value_or(1);

            // Parse color
            auto const colorVal = brickTab->get_array_of<int64_t>("color");
            if (colorVal && colorVal->size() == 3) {
                brick.color = Color(colorVal->at(0), colorVal->at(1), colorVal->at(2), 255);
            }
            else if (colorVal && colorVal->size() == 4) {
                brick.color = Color(colorVal->at(0), colorVal->at(1), colorVal->at(2), colorVal->at(3));
            }

            // Parse explicit power-ups list or single
            auto powerUpsArr = brickTab->get_array_of<std::string>("powerUps");
            if (powerUpsArr) {
                for (const auto& pu : *powerUpsArr) {
                    if (auto parsed = powerUpFromString(pu)) {
                        brick.containedPowerUps.push_back(*parsed);
                    }
                }
            } else if (auto single = brickTab->get_as<std::string>("powerUp")) {
                if (auto parsed = powerUpFromString(*single)) {
                    brick.containedPowerUps.push_back(*parsed);
                }
            }

            // Parse random drop chance
            brick.powerUpDropChance = static_cast<float>(brickTab->get_as<double>("powerUpDropChance").value_or(0.0));

            bricks.push_back(brick);
        }

        return bricks;
    }
}
