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
    using CountdownId = std::uint32_t;

    class TimerSystem;

    class TimerHandle {
    public:
        TimerHandle() = default;

        [[nodiscard]] TimerId id() const { return id_; }
        [[nodiscard]] bool valid() const { return owner_ != nullptr && id_ != 0; }
        explicit operator bool() const { return valid(); }
        [[nodiscard]] bool active() const;
        [[nodiscard]] bool cancel() const;

    private:
        TimerHandle(TimerSystem* owner, const TimerId id) :
            owner_(owner), id_(id) {}

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
        [[nodiscard]] bool active() const;
        [[nodiscard]] bool cancel() const;

    private:
        CooldownHandle(TimerSystem* owner, const CooldownId id) :
            owner_(owner), id_(id) {}

        TimerSystem* owner_ = nullptr;
        CooldownId id_ = 0;

        friend class TimerSystem;
    };

    class CountdownHandle {
    public:
        CountdownHandle() = default;

        [[nodiscard]] CountdownId id() const { return id_; }
        [[nodiscard]] bool valid() const { return owner_ != nullptr && id_ != 0; }
        explicit operator bool() const { return valid(); }
        [[nodiscard]] bool active() const;
        [[nodiscard]] bool cancel() const;

    private:
        CountdownHandle(TimerSystem* owner, const CountdownId id) :
            owner_(owner), id_(id) {}

        TimerSystem* owner_ = nullptr;
        CountdownId id_ = 0;

        friend class TimerSystem;
    };

    class TimerSystem {
    public:
        template <class Rep, class Period>
        TimerHandle after(std::chrono::duration<Rep, Period> delay, std::function<void()> callback) {
            return addTimer_(std::chrono::duration_cast<TimerDuration>(delay), false, std::nullopt,
                             std::move(callback));
        }

        TimerHandle after(const float seconds, std::function<void()> callback) {
            return after(std::chrono::duration<float>(seconds), std::move(callback));
        }

        template <class Rep, class Period>
        TimerHandle every(std::chrono::duration<Rep, Period> interval, std::function<void()> callback) {
            return addTimer_(std::chrono::duration_cast<TimerDuration>(interval), true, std::nullopt,
                             std::move(callback));
        }

        TimerHandle every(const float intervalSeconds, std::function<void()> callback) {
            return every(std::chrono::duration<float>(intervalSeconds), std::move(callback));
        }

        template <class Rep, class Period>
        TimerHandle every(std::chrono::duration<Rep, Period> interval, std::uint32_t maxRepetitions,
                          std::function<void()> callback) {
            if (maxRepetitions == 0)
                return {};

            return addTimer_(
                std::chrono::duration_cast<TimerDuration>(interval), true, maxRepetitions, std::move(callback)
                );
        }

        TimerHandle every(const float intervalSeconds, const std::uint32_t maxRepetitions,
                          std::function<void()> callback) {
            return every(std::chrono::duration<float>(intervalSeconds), maxRepetitions, std::move(callback));
        }

        bool active(const TimerHandle& handle) const;
        bool cancel(const TimerHandle& handle);
        void clearTimers();

        template <class Rep, class Period>
        CooldownHandle cooldown(std::chrono::duration<Rep, Period> duration) {
            return addCooldown_(std::chrono::duration_cast<TimerDuration>(duration));
        }

        CooldownHandle cooldown(const float durationSeconds) {
            return cooldown(std::chrono::duration<float>(durationSeconds));
        }

        bool ready(const CooldownHandle& handle) const;
        bool consume(const CooldownHandle& handle);
        bool reset(const CooldownHandle& handle);
        bool active(const CooldownHandle& handle) const;
        bool cancel(const CooldownHandle& handle);
        void clearCooldowns();

        template <class Rep, class Period>
        CountdownHandle countdown(std::chrono::duration<Rep, Period> duration,
                                  std::function<void(float)> onTick,
                                  std::function<void()> onComplete,
                                  std::optional<TimerDuration> tickInterval = std::nullopt) {
            return addCountdown_(std::chrono::duration_cast<TimerDuration>(duration), std::move(onTick),
                                 std::move(onComplete), std::move(tickInterval));
        }

        CountdownHandle countdown(const float durationSeconds,
                                  std::function<void(float)> onTick,
                                  std::function<void()> onComplete,
                                  std::optional<TimerDuration> tickInterval = std::nullopt) {
            return countdown(std::chrono::duration<float>(durationSeconds), std::move(onTick), std::move(onComplete),
                             std::move(tickInterval));
        }

        bool active(const CountdownHandle& handle) const;
        bool cancel(const CountdownHandle& handle);
        void clearCountdowns();

        void clearAll();
        void update(float dt);

    private:
        TimerHandle addTimer_(
            TimerDuration interval, bool repeating,
            std::optional<std::uint32_t> remainingRepeats,
            std::function<void()> callback);
        CooldownHandle addCooldown_(TimerDuration duration);
        CountdownHandle addCountdown_(TimerDuration duration,
                                      std::function<void(float)> onTick,
                                      std::function<void()> onComplete,
                                      std::optional<TimerDuration> tickInterval);
        void updateTimers_(TimerDuration td);
        void updateCooldowns_(TimerDuration td);
        void updateCountdowns_(TimerDuration td);
        bool cancelTimerById_(TimerId id);
        bool cancelCooldownById_(CooldownId id);
        bool cancelCountdownById_(CountdownId id);

    private:
        struct Timer {
            TimerDuration remaining;
            TimerDuration interval;
            bool repeating;
            std::optional<std::uint32_t> remainingRepeats;
            std::function<void()> callback;
        };

        struct Cooldown {
            TimerDuration duration{TimerDuration::zero()};
            TimerDuration remaining{TimerDuration::zero()};
        };

        struct Countdown {
            TimerDuration duration{TimerDuration::zero()};
            TimerDuration remaining{TimerDuration::zero()};
            std::optional<TimerDuration> tickInterval;
            TimerDuration tickAccumulator{TimerDuration::zero()};
            std::function<void(float)> onTick;
            std::function<void()> onComplete;
        };

        std::unordered_map<TimerId, Timer> timers_;
        std::unordered_map<CooldownId, Cooldown> cooldowns_;
        std::unordered_map<CountdownId, Countdown> countdowns_;

        TimerId nextTimerId_{1};
        CooldownId nextCooldownId_{1};
        CountdownId nextCountdownId_{1};
    };

    class TimerComponent final : public Component {
    public:
        explicit TimerComponent(Entity& e) :
            Component(e) {}

        template <class Rep, class Period>
        TimerHandle after(std::chrono::duration<Rep, Period> delay, std::function<void()> callback) {
            return timers_.after(delay, std::move(callback));
        }

        TimerHandle after(const float seconds, std::function<void()> callback) {
            return timers_.after(seconds, std::move(callback));
        }

        template <class Rep, class Period>
        TimerHandle every(std::chrono::duration<Rep, Period> interval, std::function<void()> callback) {
            return timers_.every(interval, std::move(callback));
        }

        TimerHandle every(const float intervalSeconds, std::function<void()> callback) {
            return timers_.every(intervalSeconds, std::move(callback));
        }

        template <class Rep, class Period>
        TimerHandle every(std::chrono::duration<Rep, Period> interval, std::uint32_t maxRepetitions,
                          std::function<void()> callback) {
            return timers_.every(interval, maxRepetitions, std::move(callback));
        }

        TimerHandle every(const float intervalSeconds, const std::uint32_t maxRepetitions, std::function<void()> callback) {
            return timers_.every(intervalSeconds, maxRepetitions, std::move(callback));
        }

        bool active(const TimerHandle& handle) const;
        bool cancel(const TimerHandle& handle);
        void clearTimers();

        template <class Rep, class Period>
        CooldownHandle cooldown(std::chrono::duration<Rep, Period> duration) {
            return timers_.cooldown(duration);
        }

        CooldownHandle cooldown(const float durationSeconds) { return timers_.cooldown(durationSeconds); }

        bool ready(const CooldownHandle& handle) const;
        bool consume(const CooldownHandle& handle);
        bool reset(const CooldownHandle& handle);
        bool active(const CooldownHandle& handle) const;
        bool cancel(const CooldownHandle& handle);
        void clearCooldowns();

        template <class Rep, class Period>
        CountdownHandle countdown(std::chrono::duration<Rep, Period> duration,
                                  std::function<void(float)> onTick,
                                  std::function<void()> onComplete,
                                  std::optional<TimerDuration> tickInterval = std::nullopt) {
            return timers_.countdown(duration, std::move(onTick), std::move(onComplete), std::move(tickInterval));
        }

        CountdownHandle countdown(float durationSeconds,
                                  std::function<void(float)> onTick,
                                  std::function<void()> onComplete,
                                  std::optional<TimerDuration> tickInterval = std::nullopt) {
            return timers_.countdown(durationSeconds, std::move(onTick), std::move(onComplete), std::move(tickInterval));
        }

        bool active(const CountdownHandle& handle) const;
        bool cancel(const CountdownHandle& handle);
        void clearCountdowns();

        void update(float dt) override;
        void clearAll();

    private:
        TimerSystem timers_{};
    };
}
