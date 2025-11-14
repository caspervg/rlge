#pragma once
#include <cstdint>
#include <vector>

namespace rlge {
    // Forward declarations
    class Engine;
    class Scene;
    class Entity;
    class Component;

    struct EntityId {
        uint32_t index = 0;
        uint32_t generation = 0;
        bool valid() const { return generation != 0; }
        bool operator==(const EntityId&) const = default;
    };

    class EntityRegistry {
    public:
        EntityId create(Entity* ptr) {
            uint32_t idx;
            if (!free_.empty()) {
                idx = free_.back();
                free_.pop_back();
                ++generations_[idx];
            }
            else {
                idx = generations_.size();
                generations_.push_back(1);
                entities_.push_back(nullptr);
            }

            entities_[idx] = ptr;
            return EntityId{idx, generations_[idx]};
        }

        void destroy(EntityId id) {
            if (!alive(id))
                return;
            entities_[id.index] = nullptr;
            free_.push_back(id.index);
        }

        Entity* get(EntityId id) const {
            if (!alive(id))
                return nullptr;
            return entities_[id.index];
        }

    private:
        std::vector<Entity*> entities_;
        std::vector<uint32_t> generations_;
        std::vector<uint32_t> free_;

        bool alive(const EntityId id) const {
            return id.index < generations_.size()
                && generations_[id.index] == id.generation
                && entities_[id.index] != nullptr;
        }
    };
}
