#include "powerup_manager.hpp"

#include <algorithm>

namespace breakout {

void PowerUpManager::update(float dt) {
    std::erase_if(effects_, [&](ActiveEffect& effect) {
        if (effect.timeRemaining <= 0.0f) {
            return false; // Permanent
        }
        effect.timeRemaining -= dt;
        if (effect.timeRemaining <= 0.0f) {
            notifyCallback(effect.type, false);
            return true;
        }
        return false;
    });
}

void PowerUpManager::activate(PowerUpType type) {
    const auto& config = kPowerUpConfigs.at(type);

    if (config.duration <= 0.0f) {
        applyInstantEffect(type);
        return;
    }

    // Refresh existing effect
    for (auto& effect : effects_) {
        if (effect.type == type) {
            effect.timeRemaining = config.duration;
            effect.magnitude = config.magnitude;
            return;
        }
    }

    // Handle mutually exclusive effects
    if (type == PowerUpType::WidePaddle) {
        deactivate(PowerUpType::NarrowPaddle);
    } else if (type == PowerUpType::NarrowPaddle) {
        deactivate(PowerUpType::WidePaddle);
    } else if (type == PowerUpType::SlowBall) {
        deactivate(PowerUpType::FastBall);
    } else if (type == PowerUpType::FastBall) {
        deactivate(PowerUpType::SlowBall);
    }

    effects_.push_back({type, config.duration, config.magnitude});
    notifyCallback(type, true);
}

void PowerUpManager::deactivate(PowerUpType type) {
    auto it = std::find_if(effects_.begin(), effects_.end(), [type](const ActiveEffect& e) { return e.type == type; });
    if (it != effects_.end()) {
        notifyCallback(type, false);
        effects_.erase(it);
    }
}

void PowerUpManager::deactivateAll() {
    for (const auto& e : effects_) {
        notifyCallback(e.type, false);
    }
    effects_.clear();
}

bool PowerUpManager::isActive(PowerUpType type) const {
    return std::ranges::any_of(effects_, [type](const ActiveEffect& e) { return e.type == type; });
}

float PowerUpManager::getMagnitude(PowerUpType type) const {
    for (const auto& e : effects_) {
        if (e.type == type) return e.magnitude;
    }
    return 1.0f;
}

float PowerUpManager::paddleWidthMultiplier() const {
    auto mult = 1.0f;
    if (isActive(PowerUpType::WidePaddle)) mult *= getMagnitude(PowerUpType::WidePaddle);
    if (isActive(PowerUpType::NarrowPaddle)) mult *= getMagnitude(PowerUpType::NarrowPaddle);
    return mult;
}

float PowerUpManager::ballSpeedMultiplier() const {
    float mult = 1.0f;
    if (isActive(PowerUpType::SlowBall)) mult *= getMagnitude(PowerUpType::SlowBall);
    if (isActive(PowerUpType::FastBall)) mult *= getMagnitude(PowerUpType::FastBall);
    return mult;
}

float PowerUpManager::scoreMultiplier() const {
    return isActive(PowerUpType::ScoreMultiplier) ? getMagnitude(PowerUpType::ScoreMultiplier) : 1.0f;
}

bool PowerUpManager::hasLaserPaddle() const { return isActive(PowerUpType::LaserPaddle); }
bool PowerUpManager::hasStickyPaddle() const { return isActive(PowerUpType::StickyPaddle); }
bool PowerUpManager::hasFireBall() const { return isActive(PowerUpType::FireBall); }
bool PowerUpManager::hasSafetyNet() const { return isActive(PowerUpType::SafetyNet); }

void PowerUpManager::applyInstantEffect(PowerUpType type) {
    notifyCallback(type, true); // Let scene handle instant effects (ExtraLife, MultiBall)
}

void PowerUpManager::notifyCallback(PowerUpType type, bool activated) {
    if (callback_) callback_(type, activated);
}

} // namespace breakout
#include <algorithm>
