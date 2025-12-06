#include "powerup_manager.hpp"

#include <algorithm>

namespace breakout {

void PowerUpManager::update(const float dt) { timers_.update(dt); }

void PowerUpManager::activate(PowerUpType type) {
    const auto& config = kPowerUpConfigs.at(type);

    if (config.duration <= 0.0f) {
        applyInstantEffect(type);
        return;
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

    refreshEffect_(type, config.duration, config.magnitude);
}

void PowerUpManager::deactivate(PowerUpType type) {
    auto it = std::find_if(effects_.begin(), effects_.end(), [type](const ActiveEffect& e) { return e.type == type; });
    if (it == effects_.end())
        return;

    if (it->countdown) {
        timers_.cancel(it->countdown);
    }
    notifyCallback(type, false);
    effects_.erase(it);
}

void PowerUpManager::deactivateAll() {
    for (const auto& e : effects_) {
        if (e.countdown) {
            timers_.cancel(e.countdown);
        }
        notifyCallback(e.type, false);
    }
    effects_.clear();
    timers_.clearCountdowns();
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
    auto mult = 1.0f;
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

void PowerUpManager::applyInstantEffect(const PowerUpType type) const {
    notifyCallback(type, true); // Let scene handle instant effects (ExtraLife, MultiBall)
}

void PowerUpManager::notifyCallback(const PowerUpType type, const bool activated) const {
    if (callback_) callback_(type, activated);
}

void PowerUpManager::refreshEffect_(const PowerUpType type, const float duration, const float magnitude) {
    for (auto& effect : effects_) {
        if (effect.type == type) {
            if (effect.countdown) {
                timers_.cancel(effect.countdown);
            }
            effect.countdown = duration > 0.0f
                                   ? timers_.countdown(duration, nullptr, [this, type]() { deactivate(type); })
                                   : rlge::CountdownHandle{};
            effect.duration = duration;
            effect.magnitude = magnitude;
            notifyCallback(type, true);
            return;
        }
    }
    addTimedEffect_(type, duration, magnitude);
}

void PowerUpManager::addTimedEffect_(const PowerUpType type, const float duration, const float magnitude) {
    ActiveEffect newEffect{
        .type = type,
        .magnitude = magnitude,
        .duration = duration,
        .countdown = {}
    };

    if (duration > 0.0f) {
        newEffect.countdown = timers_.countdown(
            duration,
            nullptr,
            [this, type]() { deactivate(type); }
        );
    }

    effects_.push_back(std::move(newEffect));
    notifyCallback(type, true);
}

} // namespace breakout
