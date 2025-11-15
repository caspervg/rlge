#pragma once
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rlge {

    // Simple type-safe event bus with immediate and queued dispatch.
    class EventBus {
    public:
        using SubscriptionId = std::size_t;

        template <typename Event>
        using Handler = std::function<void(const Event&)>;

        EventBus() = default;
        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        // Subscribe a handler for a specific event type.
        // Returns an id that can be used to unsubscribe.
        template <typename Event>
        SubscriptionId subscribe(Handler<Event> handler) {
            auto& list = handlerList<Event>();
            const SubscriptionId id = list.nextId++;
            list.handlers.push_back({id, std::move(handler)});
            return id;
        }

        // Unsubscribe a previously registered handler.
        template <typename Event>
        void unsubscribe(SubscriptionId id) {
            auto it = handlers_.find(std::type_index(typeid(Event)));
            if (it == handlers_.end())
                return;
            auto* base = it->second.get();
            auto* list = static_cast<HandlerList<Event>*>(base);
            auto& vec = list->handlers;
            for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                if (vit->id == id) {
                    vec.erase(vit);
                    break;
                }
            }
        }

        // Immediately deliver an event to all subscribers of its type.
        template <typename Event>
        void publish(const Event& ev) {
            auto it = handlers_.find(std::type_index(typeid(Event)));
            if (it == handlers_.end())
                return;
            auto* base = it->second.get();
            auto* list = static_cast<HandlerList<Event>*>(base);
            // Copy to allow handlers to subscribe/unsubscribe safely during iteration.
            auto handlersCopy = list->handlers;
            for (auto& entry : handlersCopy) {
                if (entry.fn)
                    entry.fn(ev);
            }
        }

        // Queue an event to be dispatched later via dispatchQueued().
        template <typename Event>
        void enqueue(const Event& ev) {
            queue_.emplace_back([this, ev]() { publish(ev); });
        }

        // Dispatch all queued events in FIFO order.
        void dispatchQueued() {
            if (queue_.empty())
                return;
            auto current = std::move(queue_);
            queue_.clear();
            for (auto& fn : current) {
                if (fn)
                    fn();
            }
        }

        void clear() {
            handlers_.clear();
            queue_.clear();
        }

    private:
        struct IHandlerList {
            virtual ~IHandlerList() = default;
        };

        template <typename Event>
        struct HandlerList : IHandlerList {
            struct Entry {
                SubscriptionId id;
                Handler<Event> fn;
            };
            std::vector<Entry> handlers;
            SubscriptionId nextId = 1;
        };

        template <typename Event>
        HandlerList<Event>& handlerList() {
            const std::type_index key(typeid(Event));
            auto it = handlers_.find(key);
            if (it == handlers_.end()) {
                auto list = std::make_unique<HandlerList<Event>>();
                auto* ptr = list.get();
                handlers_.emplace(key, std::move(list));
                return *ptr;
            }
            return *static_cast<HandlerList<Event>*>(it->second.get());
        }

        std::unordered_map<std::type_index, std::unique_ptr<IHandlerList>> handlers_;
        std::vector<std::function<void()>> queue_;
    };
}

