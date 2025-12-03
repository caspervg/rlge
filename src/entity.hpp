#pragma once
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "component.hpp"
#include "entity_registry.hpp"

namespace rlge {
    class Scene;
    class Component;

    class Entity {
    public:
        virtual ~Entity();

        [[nodiscard]] EntityId id() const;

        virtual void update(float dt);
        virtual void draw();

        /// Adds a component to the entity.
        /// Note: Components cannot be removed after being added. The component
        /// cache relies on this invariant to avoid cache invalidation overhead.
        template <typename T, typename... Args>
        T& add(Args&&... args) {
            static_assert(std::is_base_of_v<Component, T>, "T must be Component");
            auto ptr = std::make_unique<T>(*this, std::forward<Args>(args)...);
            T& ref = *ptr;
            components_.push_back(std::move(ptr));
            // Cache the component for O(1) lookup by type
            componentCache_[std::type_index(typeid(T))] = &ref;
            return ref;
        }

        /// Retrieves a component by type. Uses a type-indexed cache for O(1) lookup
        /// after the first access. Falls back to linear search with dynamic_cast
        /// only if the component was added via a base type or an interface type.
        /// 
        /// Cache safety: The cache stores pointers to components owned by the
        /// components_ vector. Since components cannot be removed and unique_ptr
        /// maintains stable addresses, cached pointers remain valid for the
        /// entity's lifetime.
        template <typename T>
        T* get() {
            const auto key = std::type_index(typeid(T));
            if (const auto it = componentCache_.find(key); it != componentCache_.end()) {
                return static_cast<T*>(it->second);
            }
            // Fallback: linear search (needed if component was added as base type or interface)
            for (auto& c : components_) {
                if (auto* p = dynamic_cast<T*>(c.get())) {
                    componentCache_[key] = p;  // Cache for future lookups
                    return p;
                }
            }
            return nullptr;
        }

        template <typename T>
        const T* get() const {
            const auto key = std::type_index(typeid(T));
            if (const auto it = componentCache_.find(key); it != componentCache_.end()) {
                return static_cast<const T*>(it->second);
            }
            // Fallback: linear search (needed if component was added as base type or interface)
            for (auto& c : components_) {
                if (auto* p = dynamic_cast<const T*>(c.get())) {
                    // const_cast is safe: component addresses are stable and cache is mutable
                    componentCache_[key] = const_cast<void*>(static_cast<const void*>(p));
                    return p;
                }
            }
            return nullptr;
        }

        void destroy() const;
        void destroyDeferred() const;

        Scene& scene();
        const Scene& scene() const;

    protected:
        explicit Entity(Scene& s) :
            scene_(s) {}

    private:
        friend class Scene;

        Scene& scene_;
        EntityId id_{};
        std::vector<std::unique_ptr<Component>> components_;
        mutable std::unordered_map<std::type_index, void*> componentCache_;
    };
}
