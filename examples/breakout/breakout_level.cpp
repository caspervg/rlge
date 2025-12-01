#include "breakout_level.hpp"

#include <format>

#include "breakout_entities.hpp"
#include "include/cpptoml.h"

namespace breakout {

    LevelManager::LevelManager() :
        LevelManager("../examples/breakout/assets/levels.toml") {}

    LevelManager::LevelManager(const std::string& levelFile) {
        const auto levels = cpptoml::parse_file(levelFile);
        const auto levelsArr = levels->get_table_array("levels");

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

    const Level* LevelManager::getLevel(const int index) const {
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

    std::unordered_set<BrickConfig> LevelManager::generateBricks_(const int rows, const int columns) {
        std::unordered_set<BrickConfig> bricks;
        for (auto r = 0; r < rows; r++) {
            for (auto c = 0; c < columns; c++) {
                bricks.emplace(BrickConfig{c, r, 1, LIME, Standard});
            }
        }
        return bricks;
    }

    std::unordered_set<BrickConfig> LevelManager::parseBricks_(
        const int rows,
        const int columns,
        const std::shared_ptr<cpptoml::table_array>& bricksArr
        ) {
        std::unordered_set<BrickConfig> bricks{};
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
            bricks.emplace(brick);
        }

        return bricks;
    }
}
