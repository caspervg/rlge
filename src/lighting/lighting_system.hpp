#pragma once
#include <vector>
#include <cstddef>
#include <array>

#include "raylib.h"

namespace rlge {

    constexpr int MAX_LIGHTS = 16;

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
        LightingSystem() = default;
        ~LightingSystem();

        LightingSystem(const LightingSystem&) = delete;
        LightingSystem& operator=(const LightingSystem&) = delete;

        // Initialize lighting system with render target dimensions
        void init(int width, int height);

        // Resize render textures when window size changes
        void resize(int width, int height);

        // Light management
        size_t addPointLight(Vector2 pos, float radius, Color color, float intensity);
        PointLight* getLight(size_t index);
        void removeLight(size_t index);
        void clearLights();

        // Ambient light
        void setAmbient(Color color);
        [[nodiscard]] const AmbientLight& ambient() const { return ambient_; }

        // Rendering pipeline
        void beginFrame();
        void renderLights(const Camera2D& camera);
        void applyLighting(Texture2D sceneTexture);

        // Shader access
        [[nodiscard]] const Shader& normalMapShader() const { return normalMapShader_; }

        // Light data access for normal map shader
        [[nodiscard]] const std::vector<PointLight>& lights() const { return lights_; }

        // Cached uniform locations for normal map shader
        [[nodiscard]] const NormalMapUniformLocations& normalMapLocations() const { return normalMapLocs_; }

        // Get window dimensions
        [[nodiscard]] int width() const { return width_; }
        [[nodiscard]] int height() const { return height_; }

    private:
        void loadShaders();
        void unloadShaders();
        void createRenderTextures(int width, int height);
        void destroyRenderTextures();
        void cacheUniformLocations();

        std::vector<PointLight> lights_;
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
