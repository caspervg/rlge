#pragma once
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "entity_registry.hpp"
#include "component.hpp"

namespace rlge {
    class Scene;
    class Component;

    class Entity {
    public:
        virtual ~Entity();

        EntityId id() const;

        virtual void update(float dt);
        virtual void draw();

        template <typename T, typename... Args>
        T& add(Args&&... args) {
            static_assert(std::is_base_of_v<Component, T>, "T must be Component");
            auto ptr = std::make_unique<T>(*this, std::forward<Args>(args)...);
            T& ref = *ptr;
            components_.push_back(std::move(ptr));
            return ref;
        }

        template <typename T>
        T* get() {
            for (auto& c : components_) {
                if (auto* p = dynamic_cast<T*>(c.get()))
                    return p;
            }
            return nullptr;
        }

        template <typename T>
        const T* get() const {
            for (auto& c : components_) {
                if (auto* p = dynamic_cast<const T*>(c.get()))
                    return p;
            }
            return nullptr;
        }

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
    };
}
