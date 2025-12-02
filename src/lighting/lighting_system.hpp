#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "raylib.h"

namespace rlge {

    constexpr auto MAX_LIGHTS = 16;

    struct LightId {
        std::uint16_t index{0};
        std::uint16_t generation{0};

        [[nodiscard]] bool valid() const { return generation != 0xFFFFu; }

        static LightId invalid() { return {0, 0xFFFFu}; }
    };

    struct PointLight {
        Vector2 position{0.0f, 0.0f};
        Color color{255, 255, 255, 255};
        float radius{100.0f};
        float intensity{1.0f};
        bool enabled{true};
    };

    struct AmbientLight {
        Color color{50, 50, 50, 255};
    };

    // Cached uniform locations for normal map shader
    struct NormalMapUniformLocations {
        std::array<int, MAX_LIGHTS> lightPos;
        std::array<int, MAX_LIGHTS> lightColor;
        std::array<int, MAX_LIGHTS> lightRadius;
        std::array<int, MAX_LIGHTS> lightIntensity;
        int lightCount{-1};
        int ambient{-1};
        int resolution{-1};
        int normalMap{-1};
    };

    class LightingSystem {
    public:
        struct ActiveLight {
            LightId id;
            const PointLight* light{};
        };

        struct NormalPassData {
            std::vector<ActiveLight> lights;
            AmbientLight ambient;
            const NormalMapUniformLocations* locs{nullptr};
            float winWidth{0.0f};
            float winHeight{0.0f};
            int enabledLightCount{0};
        };

        LightingSystem() = default;
        ~LightingSystem();

        LightingSystem(const LightingSystem&) = delete;
        LightingSystem& operator=(const LightingSystem&) = delete;

        // Initialize lighting system with render target dimensions
        void init(int width, int height);

        // Resize render textures when window size changes
        void resize(int width, int height);

        // Light management
        LightId addPointLight(Vector2 pos, float radius, Color color, float intensity);
        PointLight* getLight(LightId id);
        void removeLight(LightId id);
        void clearLights();

        // Ambient light
        void setAmbient(Color color);
        [[nodiscard]] const AmbientLight& ambient() const { return ambient_; }

        // Rendering pipeline
        void beginFrame() const;
        void renderLights(const Camera2D& camera, const Rectangle& viewport) const;
        void applyLighting(const Texture2D& sceneTexture) const;

        // Shader access
        [[nodiscard]] const Shader& normalMapShader() const { return normalMapShader_; }

        // Light data access for normal map shader
        [[nodiscard]] std::vector<ActiveLight> activeLights() const;

        // Cached uniform locations for normal map shader
        [[nodiscard]] const NormalMapUniformLocations& normalMapLocations() const { return normalMapLocs_; }
        [[nodiscard]] NormalPassData normalPassData() const;

        // Get window dimensions
        [[nodiscard]] int width() const { return width_; }
        [[nodiscard]] int height() const { return height_; }

    private:
        void loadShaders_();
        void unloadShaders_() const;
        void createRenderTextures_(int width, int height);
        void destroyRenderTextures_();
        void cacheUniformLocations_();

        std::array<PointLight, MAX_LIGHTS> lights_{};
        std::array<std::uint16_t, MAX_LIGHTS> generations_{};
        std::array<bool, MAX_LIGHTS> active_{};
        std::vector<std::size_t> freeList_;
        AmbientLight ambient_;

        RenderTexture2D lightBuffer_{};
        bool initialized_{false};
        int width_{0};
        int height_{0};

        // Shaders
        Shader lightAccumShader_{};
        Shader normalMapShader_{};
        Shader combineShader_{};

        // Cached uniform locations for light accumulation shader
        int lightAccumLoc_lightPos_{-1};
        int lightAccumLoc_lightColor_{-1};
        int lightAccumLoc_lightRadius_{-1};
        int lightAccumLoc_lightIntensity_{-1};
        int lightAccumLoc_resolution_{-1};

        // Cached uniform locations for normal map shader
        NormalMapUniformLocations normalMapLocs_;

        // Cached uniform locations for combine shader
        int combineLoc_lightBuffer_{-1};
        int combineLoc_ambient_{-1};
    };

} // namespace rlge
