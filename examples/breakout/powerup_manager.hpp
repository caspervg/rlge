#pragma once
#include <functional>
#include <vector>

#include "powerup_types.hpp"

namespace breakout {

struct ActiveEffect {
    PowerUpType type;
    float timeRemaining; // <= 0 means permanent until cleared
    float magnitude;
};

class PowerUpManager {
public:
    using EffectCallback = std::function<void(PowerUpType, bool activated)>; // scene-level notifications

    void update(float dt);

    // Apply a power-up (called when paddle catches one)
    void activate(PowerUpType type);

    // Deactivate all effects of a type (e.g., on ball lost)
    void deactivate(PowerUpType type);
    void deactivateAll();

    // Queries
    [[nodiscard]] bool isActive(PowerUpType type) const;
    [[nodiscard]] float getMagnitude(PowerUpType type) const; // 1.0 if not active
    [[nodiscard]] const std::vector<ActiveEffect>& activeEffects() const { return effects_; }

    // Computed properties for entities
    [[nodiscard]] float paddleWidthMultiplier() const;
    [[nodiscard]] float ballSpeedMultiplier() const;
    [[nodiscard]] float scoreMultiplier() const;
    [[nodiscard]] bool hasLaserPaddle() const;
    [[nodiscard]] bool hasStickyPaddle() const;
    [[nodiscard]] bool hasFireBall() const;
    [[nodiscard]] bool hasSafetyNet() const;

    void setCallback(EffectCallback cb) { callback_ = std::move(cb); }

private:
    void applyInstantEffect(PowerUpType type) const;
    void notifyCallback(PowerUpType type, bool activated) const;

private:
    std::vector<ActiveEffect> effects_;
    EffectCallback callback_;
};

} // namespace breakout
