#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"

namespace rlge {

    // Type traits for shader uniform types
    template<typename T>
    struct ShaderUniformType;

    template<>
    struct ShaderUniformType<float> {
        static constexpr int value = SHADER_UNIFORM_FLOAT;
        static constexpr int count = 1;
    };

    template<>
    struct ShaderUniformType<Vector2> {
        static constexpr int value = SHADER_UNIFORM_VEC2;
        static constexpr int count = 1;
    };

    template<>
    struct ShaderUniformType<Vector3> {
        static constexpr int value = SHADER_UNIFORM_VEC3;
        static constexpr int count = 1;
    };

    template<>
    struct ShaderUniformType<Vector4> {
        static constexpr int value = SHADER_UNIFORM_VEC4;
        static constexpr int count = 1;
    };

    template<>
    struct ShaderUniformType<int> {
        static constexpr int value = SHADER_UNIFORM_INT;
        static constexpr int count = 1;
    };

    // Type-erased binding for applying uniforms
    struct UniformBinding {
        int location = -1;
        std::function<void(Shader, int)> apply;
    };

    // Type-safe shader parameters template
    template<typename T>
    class ShaderParams {
    public:
        explicit ShaderParams(Shader shader)
            : shader_(shader)
            , params_() {}

        // Copy constructor
        ShaderParams(const ShaderParams& other)
            : shader_(other.shader_)
            , params_(other.params_)
            , bindings_(other.bindings_)
            , bindingSetups_(other.bindingSetups_) {
            // Re-bind to ensure pointers reference our params_
            rebind();
        }

        // Move constructor
        ShaderParams(ShaderParams&& other) noexcept
            : shader_(other.shader_)
            , params_(std::move(other.params_))
            , bindings_(std::move(other.bindings_))
            , bindingSetups_(std::move(other.bindingSetups_)) {
            // Re-bind to ensure pointers reference our params_
            rebind();
        }

        // Copy assignment
        ShaderParams& operator=(const ShaderParams& other) {
            if (this != &other) {
                shader_ = other.shader_;
                params_ = other.params_;
                bindings_ = other.bindings_;
                bindingSetups_ = other.bindingSetups_;
                rebind();
            }
            return *this;
        }

        // Move assignment
        ShaderParams& operator=(ShaderParams&& other) noexcept {
            if (this != &other) {
                shader_ = other.shader_;
                params_ = std::move(other.params_);
                bindings_ = std::move(other.bindings_);
                bindingSetups_ = std::move(other.bindingSetups_);
                rebind();
            }
            return *this;
        }

        // Fluent bind API using pointer-to-member
        template<typename M>
        ShaderParams& bind(const char* uniformName, M T::* member) {
            // Cache the uniform location
            const int location = GetShaderLocation(shader_, uniformName);

            // Store the binding setup for rebinding after copy/move
            bindingSetups_.push_back([this, uniformName, member]() {
                const int loc = GetShaderLocation(shader_, uniformName);
                UniformBinding binding;
                binding.location = loc;
                binding.apply = [this, member, loc](Shader s, int) {
                    if (loc < 0) return;
                    const M& value = params_.*member;
                    SetShaderValue(s, loc, &value,
                                   ShaderUniformType<M>::value);
                };
                bindings_.push_back(std::move(binding));
            });

            // Create the binding immediately
            UniformBinding binding;
            binding.location = location;
            binding.apply = [this, member, location](Shader s, int) {
                if (location < 0) return;
                const M& value = params_.*member;
                SetShaderValue(s, location, &value,
                               ShaderUniformType<M>::value);
            };
            bindings_.push_back(std::move(binding));

            return *this;
        }

        // Arrow operator for direct typed access
        T* operator->() { return &params_; }
        const T* operator->() const { return &params_; }

        // Reference access
        T& params() { return params_; }
        const T& params() const { return params_; }

        // Apply all bound uniforms to the shader
        void apply() {
            for (auto& binding : bindings_) {
                if (binding.apply) {
                    binding.apply(shader_, binding.location);
                }
            }
        }

        // Get the underlying shader
        Shader shader() const { return shader_; }

    private:
        void rebind() {
            // Clear existing bindings and recreate from setups
            bindings_.clear();
            auto setups = std::move(bindingSetups_);
            for (auto& setup : setups) {
                setup();
            }
        }

        Shader shader_;
        T params_;
        std::vector<UniformBinding> bindings_;
        std::vector<std::function<void()>> bindingSetups_;
    };

    // Interface for type-erased shader params access
    class IShaderParams {
    public:
        virtual ~IShaderParams() = default;
        virtual void apply() = 0;
        virtual Shader shader() const = 0;
    };

    // Type-erased wrapper for ShaderParams
    template<typename T>
    class ShaderParamsWrapper : public IShaderParams {
    public:
        explicit ShaderParamsWrapper(ShaderParams<T> params)
            : params_(std::move(params)) {}

        void apply() override { params_.apply(); }
        Shader shader() const override { return params_.shader(); }

        ShaderParams<T>& get() { return params_; }
        const ShaderParams<T>& get() const { return params_; }

    private:
        ShaderParams<T> params_;
    };

} // namespace rlge
