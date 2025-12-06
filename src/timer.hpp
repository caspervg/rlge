#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>

#include "component.hpp"

namespace rlge {
    using TimerId = std::uint32_t;
    using TimerDuration = std::chrono::duration<float, std::milli>;
    using CooldownId = std::uint32_t;

    class TimerSystem;

    class TimerHandle {
    public:
        TimerHandle() = default;

        [[nodiscard]] TimerId id() const { return id_; }
        [[nodiscard]] bool valid() const { return owner_ != nullptr && id_ != 0; }
        explicit operator bool() const { return valid(); }
        [[nodiscard]] bool cancel() const;

    private:
        TimerHandle(TimerSystem* owner, TimerId id) : owner_(owner), id_(id) {}

        TimerSystem* owner_ = nullptr;
        TimerId id_ = 0;

        friend class TimerSystem;
    };

    class CooldownHandle {
    public:
        CooldownHandle() = default;

        [[nodiscard]] CooldownId id() const { return id_; }
        [[nodiscard]] bool valid() const { return owner_ != nullptr && id_ != 0; }
        explicit operator bool() const { return valid(); }

        [[nodiscard]] bool ready() const;
        [[nodiscard]] bool consume() const;
        [[nodiscard]] bool reset() const;
        [[nodiscard]] bool clear() const;

    private:
        CooldownHandle(TimerSystem* owner, CooldownId id) : owner_(owner), id_(id) {}

        TimerSystem* owner_ = nullptr;
        CooldownId id_ = 0;

        friend class TimerSystem;
    };

    class TimerSystem {
    public:
        template <class Rep, class Period>
        TimerHandle after(std::chrono::duration<Rep, Period> delay, std::function<void()> callback) {
            return addTimer(std::chrono::duration_cast<TimerDuration>(delay), false, std::nullopt, std::move(callback));
        }

        TimerHandle after(float seconds, std::function<void()> callback) {
            return after(std::chrono::duration<float>(seconds), std::move(callback));
        }

        template <class Rep, class Period>
        TimerHandle every(std::chrono::duration<Rep, Period> interval, std::function<void()> callback) {
            return addTimer(std::chrono::duration_cast<TimerDuration>(interval), true, std::nullopt, std::move(callback));
        }

        TimerHandle every(float intervalSeconds, std::function<void()> callback) {
            return every(std::chrono::duration<float>(intervalSeconds), std::move(callback));
        }

        template <class Rep, class Period>
        TimerHandle every(std::chrono::duration<Rep, Period> interval, std::uint32_t maxRepetitions, std::function<void()> callback) {
            if (maxRepetitions == 0)
                return {};

            return addTimer(
                std::chrono::duration_cast<TimerDuration>(interval), true, maxRepetitions, std::move(callback)
            );
        }

        TimerHandle every(float intervalSeconds, std::uint32_t maxRepetitions, std::function<void()> callback) {
            return every(std::chrono::duration<float>(intervalSeconds), maxRepetitions, std::move(callback));
        }

        bool cancel(const TimerHandle& handle);
        void clear();

        template <class Rep, class Period>
        CooldownHandle cooldown(std::chrono::duration<Rep, Period> duration) {
            return addCooldown(std::chrono::duration_cast<TimerDuration>(duration));
        }

        CooldownHandle cooldown(float durationSeconds) { return cooldown(std::chrono::duration<float>(durationSeconds)); }

        bool ready(const CooldownHandle& handle) const;
        bool consume(const CooldownHandle& handle);
        bool reset(const CooldownHandle& handle);
        bool clear(const CooldownHandle& handle);
        void clearCooldowns();

        void update(float dt);

    private:
        struct Timer {
            TimerDuration remaining;
            TimerDuration interval;
            bool repeating;
            std::optional<std::uint32_t> remainingRepeats;
            std::function<void()> callback;
        };

        TimerHandle addTimer(
            TimerDuration interval, bool repeating, std::optional<std::uint32_t> remainingRepeats, std::function<void()> callback
        );
        CooldownHandle addCooldown(TimerDuration duration);
        bool cancelId(TimerId id);
        bool clearCooldownId(CooldownId id);

        std::unordered_map<TimerId, Timer> timers_;
        struct Cooldown {
            TimerDuration duration{TimerDuration::zero()};
            TimerDuration remaining{TimerDuration::zero()};
        };
        std::unordered_map<CooldownId, Cooldown> cooldowns_;
        TimerId nextId_{1};
        CooldownId nextCooldownId_{1};
    };

    class TimerComponent final : public Component {
    public:
        explicit TimerComponent(Entity& e) : Component(e) {}

        template <class Rep, class Period>
        TimerHandle after(std::chrono::duration<Rep, Period> delay, std::function<void()> callback) {
            return timers_.after(delay, std::move(callback));
        }

        TimerHandle after(float seconds, std::function<void()> callback) { return timers_.after(seconds, std::move(callback)); }

        template <class Rep, class Period>
        TimerHandle every(std::chrono::duration<Rep, Period> interval, std::function<void()> callback) {
            return timers_.every(interval, std::move(callback));
        }

        TimerHandle every(float intervalSeconds, std::function<void()> callback) {
            return timers_.every(intervalSeconds, std::move(callback));
        }

        template <class Rep, class Period>
        TimerHandle every(std::chrono::duration<Rep, Period> interval, std::uint32_t maxRepetitions, std::function<void()> callback) {
            return timers_.every(interval, maxRepetitions, std::move(callback));
        }

        TimerHandle every(float intervalSeconds, std::uint32_t maxRepetitions, std::function<void()> callback) {
            return timers_.every(intervalSeconds, maxRepetitions, std::move(callback));
        }

        bool cancel(const TimerHandle& handle);
        void clear();

        template <class Rep, class Period>
        CooldownHandle cooldown(std::chrono::duration<Rep, Period> duration) {
            return timers_.cooldown(duration);
        }

        CooldownHandle cooldown(float durationSeconds) { return timers_.cooldown(durationSeconds); }

        bool ready(const CooldownHandle& handle) const;
        bool consume(const CooldownHandle& handle);
        bool reset(const CooldownHandle& handle);
        bool clear(const CooldownHandle& handle);
        void clearCooldowns();

        void update(float dt) override;

    private:
        TimerSystem timers_{};
    };
}

