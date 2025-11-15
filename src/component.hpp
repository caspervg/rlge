#pragma once
namespace rlge {
    class Entity;

    class Component {
    public:
        virtual ~Component() = default;
        virtual void update(const float) {}
        virtual void draw() {}

        Entity& entity() { return entity_; }
        const Entity& entity() const { return entity_; }

    protected:
        explicit Component(Entity& e) :
            entity_(e) {}

    private:
        Entity& entity_;
    };
}
