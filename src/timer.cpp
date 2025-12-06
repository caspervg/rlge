#include "timer.hpp"

#include <vector>

namespace rlge {
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

    bool CooldownHandle::clear() const {
        if (!owner_)
            return false;
        return owner_->clear(*this);
    }

    bool TimerSystem::cancel(const TimerHandle& handle) {
        if (handle.owner_ != this)
            return false;
        return cancelId(handle.id_);
    }

    bool TimerSystem::cancelId(const TimerId id) {
        return timers_.erase(id) > 0;
    }

    void TimerSystem::clear() {
        timers_.clear();
        cooldowns_.clear();
    }

    TimerHandle TimerSystem::addTimer(
        const TimerDuration interval, const bool repeating, std::optional<std::uint32_t> remainingRepeats, std::function<void()> callback
    ) {
        TimerId id = nextId_++;
        timers_.emplace(id, Timer{interval, interval, repeating, remainingRepeats, std::move(callback)});
        return TimerHandle{this, id};
    }

    CooldownHandle TimerSystem::addCooldown(const TimerDuration duration) {
        CooldownId id = nextCooldownId_++;
        cooldowns_.emplace(id, Cooldown{duration, TimerDuration::zero()});
        return CooldownHandle{this, id};
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

    bool TimerSystem::clear(const CooldownHandle& handle) {
        if (handle.owner_ != this)
            return false;
        return clearCooldownId(handle.id_);
    }

    bool TimerSystem::clearCooldownId(const CooldownId id) { return cooldowns_.erase(id) > 0; }

    void TimerSystem::clearCooldowns() {
        cooldowns_.clear();
    }

    void TimerSystem::update(const float dtSeconds) {
        const auto dtMillis = std::chrono::duration_cast<TimerDuration>(std::chrono::duration<float>(dtSeconds));

        std::vector<TimerId> timerIds;
        timerIds.reserve(timers_.size());
        for (const auto& [id, _] : timers_) {
            timerIds.push_back(id);
        }

        std::vector<TimerId> timersToRemove;
        for (const auto id : timerIds) {
            const auto it = timers_.find(id);
            if (it == timers_.end())
                continue;

            auto& timer = it->second;
            timer.remaining -= dtMillis;

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

        for (auto& [_, cooldown] : cooldowns_) {
            if (cooldown.remaining > TimerDuration::zero()) {
                cooldown.remaining -= dtMillis;
            }
        }
    }

    bool TimerComponent::cancel(const TimerHandle& handle) {
        return timers_.cancel(handle);
    }

    void TimerComponent::clear() {
        timers_.clear();
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

    bool TimerComponent::clear(const CooldownHandle& handle) {
        return timers_.clear(handle);
    }

    void TimerComponent::clearCooldowns() {
        timers_.clearCooldowns();
    }

    void TimerComponent::update(const float dtSeconds) {
        Component::update(dtSeconds);
        timers_.update(dtSeconds);
    }
}

