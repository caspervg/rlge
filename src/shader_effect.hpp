#pragma once
#include "component.hpp"
#include "shader_params.hpp"

namespace rlge {

    // Interface for type-erased shader effect access
    struct HasShaderEffect {
        virtual ~HasShaderEffect() = default;
        virtual void apply() = 0;
        virtual Shader shader() const = 0;
    };

    // ShaderEffect component for per-entity shader effects
    // This bypasses batching for custom shader effects
    template<typename T>
    class ShaderEffect : public Component, public HasShaderEffect {
    public:
        ShaderEffect(Entity& e, Shader shader)
            : Component(e)
            , params_(shader) {}

        // Fluent bind API
        template<typename M>
        ShaderEffect& bind(const char* uniformName, M T::* member) {
            params_.bind(uniformName, member);
            return *this;
        }

        // Access to typed parameters
        T& params() { return params_.params(); }
        const T& params() const { return params_.params(); }

        // Arrow operator for direct access
        T* operator->() { return params_.operator->(); }
        const T* operator->() const { return params_.operator->(); }

        // HasShaderEffect interface
        void apply() override { params_.apply(); }
        Shader shader() const override { return params_.shader(); }

        // Component interface - no update needed by default
        void update(float) override {}
        void draw() override {}

    private:
        ShaderParams<T> params_;
    };

} // namespace rlge
