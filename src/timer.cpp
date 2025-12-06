#include "timer.hpp"

#include <algorithm>
#include <ranges>
#include <vector>

namespace rlge {
    bool TimerHandle::active() const {
        if (!owner_)
            return false;
        return owner_->active(*this);
    }

    bool TimerHandle::cancel() const {
        if (!owner_)
            return false;
        return owner_->cancel(*this);
    }

    bool CooldownHandle::ready() const {
        if (!owner_)
            return false;
        return owner_->ready(*this);
    }

    bool CooldownHandle::consume() const {
        if (!owner_)
            return false;
        return owner_->consume(*this);
    }

    bool CooldownHandle::reset() const {
        if (!owner_)
            return false;
        return owner_->reset(*this);
    }

    bool CooldownHandle::active() const {
        if (!owner_)
            return false;
        return owner_->active(*this);
    }

    bool CooldownHandle::cancel() const {
        if (!owner_)
            return false;
        return owner_->cancel(*this);
    }

    bool CountdownHandle::active() const {
        if (!owner_)
            return false;
        return owner_->active(*this);
    }

    bool CountdownHandle::cancel() const {
        if (!owner_)
            return false;
        return owner_->cancel(*this);
    }

    bool TimerSystem::active(const TimerHandle& handle) const {
        if (handle.owner_ != this)
            return false;
        return timers_.contains(handle.id_);
    }

    bool TimerSystem::cancel(const TimerHandle& handle) {
        if (handle.owner_ != this)
            return false;
        return cancelTimerById_(handle.id_);
    }

    void TimerSystem::clearTimers() {
        timers_.clear();
    }

    bool TimerSystem::ready(const CooldownHandle& handle) const {
        if (handle.owner_ != this)
            return false;
        if (const auto it = cooldowns_.find(handle.id_); it != cooldowns_.end()) {
            return it->second.remaining <= TimerDuration::zero();
        }
        return false;
    }

    bool TimerSystem::consume(const CooldownHandle& handle) {
        if (handle.owner_ != this)
            return false;
        if (const auto it = cooldowns_.find(handle.id_); it != cooldowns_.end()) {
            if (it->second.remaining <= TimerDuration::zero()) {
                it->second.remaining = it->second.duration;
                return true;
            }
            return false;
        }
        return false;
    }

    bool TimerSystem::reset(const CooldownHandle& handle) {
        if (handle.owner_ != this)
            return false;
        if (const auto it = cooldowns_.find(handle.id_); it != cooldowns_.end()) {
            it->second.remaining = it->second.duration;
            return true;
        }
        return false;
    }

    bool TimerSystem::active(const CooldownHandle& handle) const {
        if (handle.owner_ != this)
            return false;
        return cooldowns_.contains(handle.id_);
    }

    bool TimerSystem::cancel(const CooldownHandle& handle) {
        if (handle.owner_ != this)
            return false;
        return cancelCooldownById_(handle.id_);
    }

    void TimerSystem::clearCooldowns() {
        cooldowns_.clear();
    }

    bool TimerSystem::active(const CountdownHandle& handle) const {
        if (handle.owner_ != this)
            return false;
        return countdowns_.contains(handle.id_);
    }

    bool TimerSystem::cancel(const CountdownHandle& handle) {
        if (handle.owner_ != this)
            return false;
        return cancelCountdownById_(handle.id_);
    }

    void TimerSystem::clearCountdowns() {
        countdowns_.clear();
    }

    void TimerSystem::clearAll() {
        clearTimers();
        clearCooldowns();
        clearCountdowns();
    }

    void TimerSystem::update(const float dtSeconds) {
        const auto dtMillis = std::chrono::duration_cast<TimerDuration>(std::chrono::duration<float>(dtSeconds));

        updateTimers_(dtMillis);
        updateCooldowns_(dtMillis);
        updateCountdowns_(dtMillis);
    }

    TimerHandle TimerSystem::addTimer_(
        const TimerDuration interval, const bool repeating, const std::optional<std::uint32_t> remainingRepeats,
        std::function<void()> callback
        ) {
        TimerId id = nextTimerId_++;
        timers_.emplace(id, Timer{interval, interval, repeating, remainingRepeats, std::move(callback)});
        return TimerHandle{this, id};
    }

    CooldownHandle TimerSystem::addCooldown_(const TimerDuration duration) {
        CooldownId id = nextCooldownId_++;
        cooldowns_.emplace(id, Cooldown{duration, TimerDuration::zero()});
        return CooldownHandle{this, id};
    }

    CountdownHandle TimerSystem::addCountdown_(const TimerDuration duration,
                                               std::function<void(float)> onTick,
                                               std::function<void()> onComplete,
                                               std::optional<TimerDuration> tickInterval) {
        CountdownId id = nextCountdownId_++;
        countdowns_.emplace(id, Countdown{
                                duration,
                                duration,
                                tickInterval,
                                TimerDuration::zero(),
                                std::move(onTick),
                                std::move(onComplete)
                            });
        return CountdownHandle{this, id};
    }

    void TimerSystem::updateTimers_(const TimerDuration td) {
        std::vector<TimerId> timerIds;
        timerIds.reserve(timers_.size());
        for (const auto& id : timers_ | std::views::keys) {
            timerIds.push_back(id);
        }

        std::vector<TimerId> timersToRemove;
        for (const auto id : timerIds) {
            const auto it = timers_.find(id);
            if (it == timers_.end())
                continue;

            auto& timer = it->second;
            timer.remaining -= td;

            while (timer.remaining <= TimerDuration::zero()) {
                if (timer.callback)
                    timer.callback();

                if (!timer.repeating || !timer.callback) {
                    timersToRemove.push_back(id);
                    break;
                }

                if (timer.remainingRepeats) {
                    if (*timer.remainingRepeats > 0) {
                        --(*timer.remainingRepeats);
                    }

                    if (timer.remainingRepeats.value_or(0) == 0) {
                        timersToRemove.push_back(id);
                        break;
                    }
                }

                timer.remaining += timer.interval;
            }
        }

        for (const auto id : timersToRemove) {
            timers_.erase(id);
        }
    }

    void TimerSystem::updateCooldowns_(const TimerDuration td) {
        for (auto& [duration, remaining] : cooldowns_ | std::views::values) {
            if (remaining > TimerDuration::zero()) {
                remaining -= td;
            }
        }
    }

    void TimerSystem::updateCountdowns_(TimerDuration td) {
        std::vector<CountdownId> countdownIds;
        countdownIds.reserve(countdowns_.size());
        for (const auto& id : countdowns_ | std::views::keys) {
            countdownIds.push_back(id);
        }

        std::vector<CountdownId> countdownsToRemove;
        for (const auto id : countdownIds) {
            const auto it = countdowns_.find(id);
            if (it == countdowns_.end())
                continue;

            auto& countdown = it->second;
            countdown.remaining -= td;
            countdown.tickAccumulator += td;

            const float remainingSeconds = std::max(countdown.remaining.count(), 0.0f) / 1000.0f;

            if (countdown.onTick) {
                if (countdown.tickInterval) {
                    while (countdown.tickAccumulator >= *countdown.tickInterval && countdown.remaining >
                        TimerDuration::zero()) {
                        countdown.tickAccumulator -= *countdown.tickInterval;
                        countdown.onTick(remainingSeconds);
                    }
                }
                else {
                    countdown.onTick(remainingSeconds);
                    countdown.tickAccumulator = TimerDuration::zero();
                }
            }

            if (countdown.remaining <= TimerDuration::zero()) {
                if (countdown.onComplete)
                    countdown.onComplete();
                countdownsToRemove.push_back(id);
            }
        }

        for (const auto id : countdownsToRemove) {
            countdowns_.erase(id);
        }
    }

    bool TimerSystem::cancelTimerById_(const TimerId id) {
        return timers_.erase(id) > 0;
    }

    bool TimerSystem::cancelCooldownById_(const CooldownId id) { return cooldowns_.erase(id) > 0; }

    bool TimerSystem::cancelCountdownById_(const CountdownId id) { return countdowns_.erase(id) > 0; }

    bool TimerComponent::active(const TimerHandle& handle) const {
        return timers_.active(handle);
    }

    bool TimerComponent::cancel(const TimerHandle& handle) {
        return timers_.cancel(handle);
    }

    void TimerComponent::clearTimers() {
        timers_.clearTimers();
    }

    bool TimerComponent::ready(const CooldownHandle& handle) const {
        return timers_.ready(handle);
    }

    bool TimerComponent::consume(const CooldownHandle& handle) {
        return timers_.consume(handle);
    }

    bool TimerComponent::reset(const CooldownHandle& handle) {
        return timers_.reset(handle);
    }

    bool TimerComponent::active(const CooldownHandle& handle) const {
        return timers_.active(handle);
    }

    bool TimerComponent::cancel(const CooldownHandle& handle) {
        return timers_.cancel(handle);
    }

    void TimerComponent::clearCooldowns() {
        timers_.clearCooldowns();
    }

    bool TimerComponent::active(const CountdownHandle& handle) const {
        return timers_.active(handle);
    }

    bool TimerComponent::cancel(const CountdownHandle& handle) {
        return timers_.cancel(handle);
    }

    void TimerComponent::clearCountdowns() {
        timers_.clearCountdowns();
    }

    void TimerComponent::update(const float dtSeconds) {
        Component::update(dtSeconds);
        timers_.update(dtSeconds);
    }

    void TimerComponent::clearAll() {
        timers_.clearAll();
    }
}
