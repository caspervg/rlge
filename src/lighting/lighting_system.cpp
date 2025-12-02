#include "lighting_system.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace rlge {

// Light accumulation fragment shader - renders radial gradient lights
static auto lightAccumFragmentShader = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform vec2 u_lightPos;
uniform vec3 u_lightColor;
uniform float u_lightRadius;
uniform float u_lightIntensity;
uniform vec2 u_resolution;

out vec4 finalColor;

void main() {
    vec2 screenPos = fragTexCoord * u_resolution;
    vec2 fragPos = vec2(gl_FragCoord.x, u_resolution.y - gl_FragCoord.y);
    float dist = distance(fragPos, u_lightPos);
    
    // Smooth falloff
    float attenuation = 1.0 - smoothstep(0.0, u_lightRadius, dist);
    attenuation = attenuation * attenuation; // Quadratic falloff
    
    vec3 light = u_lightColor * attenuation * u_lightIntensity;
    finalColor = vec4(light, 1.0);
}
)";

// Normal map fragment shader - per-pixel lighting using normal map
static auto normalMapFragmentShader = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;     // Diffuse texture
uniform sampler2D u_normalMap;  // Normal map texture
uniform vec2 u_resolution;
uniform int u_lightCount;
uniform vec3 u_ambient;

// Light arrays (max 16 lights)
uniform vec2 u_lightPos[16];
uniform vec3 u_lightColor[16];
uniform float u_lightRadius[16];
uniform float u_lightIntensity[16];

out vec4 finalColor;

void main() {
    vec4 diffuse = texture(texture0, fragTexCoord) * fragColor;
    
    // Sample normal map and convert from [0,1] to [-1,1]
    vec3 normal = texture(u_normalMap, fragTexCoord).rgb * 2.0 - 1.0;
    normal = normalize(normal);
    
    vec2 fragPos = vec2(gl_FragCoord.x, u_resolution.y - gl_FragCoord.y);
    vec3 lighting = u_ambient;
    
    for (int i = 0; i < u_lightCount && i < 16; i++) {
        vec2 lightDir2D = u_lightPos[i] - fragPos;
        float dist = length(lightDir2D);
        
        // Attenuation
        float attenuation = 1.0 - smoothstep(0.0, u_lightRadius[i], dist);
        attenuation = attenuation * attenuation;
        
        if (attenuation > 0.0) {
            // Create 3D light direction (light is above the surface)
            vec3 lightDir3D = normalize(vec3(lightDir2D, 50.0));
            
            // Diffuse lighting using normal map
            float diff = max(dot(normal, lightDir3D), 0.0);
            
            lighting += u_lightColor[i] * diff * attenuation * u_lightIntensity[i];
        }
    }
    
    finalColor = vec4(diffuse.rgb * lighting, diffuse.a);
}
)";

// Combine fragment shader - multiplies a scene by (ambient + lightBuffer)
static auto combineFragmentShader = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;      // Scene texture
uniform sampler2D u_lightBuffer; // Light accumulation buffer
uniform vec3 u_ambient;

out vec4 finalColor;

void main() {
    vec4 sceneColor = texture(texture0, fragTexCoord);
    vec2 uv = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);
    vec3 lightColor = texture(u_lightBuffer, uv).rgb;
    
    // Combine scene with lighting (ambient + light buffer)
    vec3 totalLight = u_ambient + lightColor;
    
    // Clamp to avoid over-brightening
    totalLight = clamp(totalLight, 0.0, 2.0);
    
    finalColor = vec4(sceneColor.rgb * totalLight, sceneColor.a);
}
)";

LightingSystem::~LightingSystem() {
    if (initialized_) {
        destroyRenderTextures_();
        unloadShaders_();
    }
}

void LightingSystem::init(const int width, const int height) {
    if (initialized_) {
        resize(width, height);
        return;
    }

    width_ = width;
    height_ = height;

    clearLights();
    loadShaders_();
    cacheUniformLocations_();
    createRenderTextures_(width, height);

    initialized_ = true;
}

void LightingSystem::resize(const int width, const int height) {
    if (!initialized_) {
        return;
    }

    if (width_ == width && height_ == height) {
        return;
    }

    width_ = width;
    height_ = height;

    destroyRenderTextures_();
    createRenderTextures_(width, height);
}

LightId LightingSystem::addPointLight(const Vector2 pos, const float radius, const Color color, const float intensity) {
    std::size_t slot = std::numeric_limits<std::size_t>::max();
    if (!freeList_.empty()) {
        slot = freeList_.back();
        freeList_.pop_back();
    } else {
        for (std::size_t i = 0; i < MAX_LIGHTS; ++i) {
            if (!active_[i]) {
                slot = i;
                break;
            }
        }
    }

    if (slot == std::numeric_limits<std::size_t>::max()) {
        return LightId::invalid();
    }

    lights_[slot] = {pos, color, radius, intensity, true};
    active_[slot] = true;
    // Ensure generation is non-zero; zero is a valid generation.
    const LightId id{static_cast<std::uint16_t>(slot), generations_[slot]};
    return id;
}

PointLight* LightingSystem::getLight(const LightId id) {
    const std::size_t idx = id.index;
    if (idx >= static_cast<std::size_t>(MAX_LIGHTS)) return nullptr;
    if (!active_[idx]) return nullptr;
    if (generations_[idx] != id.generation) return nullptr;
    return &lights_[idx];
}

void LightingSystem::removeLight(const LightId id) {
    const std::size_t idx = id.index;
    if (idx >= static_cast<std::size_t>(MAX_LIGHTS)) return;
    if (!active_[idx]) return;
    if (generations_[idx] != id.generation) return;

    active_[idx] = false;
    generations_[idx] = static_cast<std::uint16_t>(generations_[idx] + 1);
    freeList_.push_back(idx);
}

void LightingSystem::clearLights() {
    for (auto i = 0; i < MAX_LIGHTS; ++i) {
        active_[i] = false;
        generations_[i] = 0;
    }
    freeList_.clear();
    for (std::size_t i = 0; i < MAX_LIGHTS; ++i) {
        freeList_.push_back(MAX_LIGHTS - 1 - i);
    }
}

void LightingSystem::setAmbient(const Color color) {
    ambient_.color = color;
}

void LightingSystem::beginFrame() const {
    if (!initialized_) return;

    // Clear light buffer to black
    BeginTextureMode(lightBuffer_);
    ClearBackground(BLACK);
    EndTextureMode();
}

void LightingSystem::renderLights(const Camera2D& camera, const Rectangle& viewport) const {
    if (!initialized_) return;
    if (viewport.width <= 0 || viewport.height <= 0) return;

    BeginTextureMode(lightBuffer_);
    BeginBlendMode(BLEND_ADDITIVE);
    BeginScissorMode(
        static_cast<int>(viewport.x),
        static_cast<int>(viewport.y),
        static_cast<int>(viewport.width),
        static_cast<int>(viewport.height)
        );

    // Set resolution uniform
    const Vector2 resolution = {static_cast<float>(width_), static_cast<float>(height_)};
    SetShaderValue(lightAccumShader_, lightAccumLoc_resolution_, &resolution, SHADER_UNIFORM_VEC2);

    for (auto i = 0; i < MAX_LIGHTS; ++i) {
        if (!active_[i]) continue;
        const auto& light = lights_[i];
        if (!light.enabled) continue;

        // Convert world position to screen position
        Vector2 screenPos = GetWorldToScreen2D(light.position, camera);

        // Set light uniforms
        SetShaderValue(lightAccumShader_, lightAccumLoc_lightPos_, &screenPos, SHADER_UNIFORM_VEC2);

        Vector3 colorVec = {
            light.color.r / 255.0f,
            light.color.g / 255.0f,
            light.color.b / 255.0f
        };
        SetShaderValue(lightAccumShader_, lightAccumLoc_lightColor_, &colorVec, SHADER_UNIFORM_VEC3);

        // Scale radius by camera zoom
        float scaledRadius = light.radius * camera.zoom;
        SetShaderValue(lightAccumShader_, lightAccumLoc_lightRadius_, &scaledRadius, SHADER_UNIFORM_FLOAT);
        SetShaderValue(lightAccumShader_, lightAccumLoc_lightIntensity_, &light.intensity, SHADER_UNIFORM_FLOAT);

        // Draw viewport quad with shader
        BeginShaderMode(lightAccumShader_);
        DrawRectangle(
            static_cast<int>(viewport.x),
            static_cast<int>(viewport.y),
            static_cast<int>(viewport.width),
            static_cast<int>(viewport.height),
            WHITE
            );
        EndShaderMode();
    }

    EndScissorMode();
    EndBlendMode();
    EndTextureMode();
}

void LightingSystem::applyLighting(const Texture2D& sceneTexture) const {
    if (!initialized_) return;

    // Set combine shader uniforms
    Vector3 ambientVec = {
        ambient_.color.r / 255.0f,
        ambient_.color.g / 255.0f,
        ambient_.color.b / 255.0f
    };
    SetShaderValue(combineShader_, combineLoc_ambient_, &ambientVec, SHADER_UNIFORM_VEC3);

    // Bind light buffer texture to texture slot 1
    SetShaderValueTexture(combineShader_, combineLoc_lightBuffer_, lightBuffer_.texture);

    // Draw scene with combine shader
    BeginShaderMode(combineShader_);

    // Handle Y-flip for render texture (Raylib convention)
    const Rectangle srcRect = {
        0.0f, 0.0f,
        static_cast<float>(sceneTexture.width),
        -static_cast<float>(sceneTexture.height)
    };
    const Rectangle destRect = {
        0.0f, 0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_)
    };

    DrawTexturePro(sceneTexture, srcRect, destRect, {0, 0}, 0.0f, WHITE);

    EndShaderMode();
}

std::vector<LightingSystem::ActiveLight> LightingSystem::activeLights() const {
    std::vector<ActiveLight> out;
    out.reserve(MAX_LIGHTS);
    for (auto i = 0; i < MAX_LIGHTS; ++i) {
        if (!active_[i]) continue;
        out.push_back(ActiveLight{
            LightId{static_cast<std::uint16_t>(i), generations_[i]},
            &lights_[i]
        });
    }
    return out;
}

LightingSystem::NormalPassData LightingSystem::normalPassData() const {
    NormalPassData data;
    data.lights = activeLights();
    data.ambient = ambient_;
    data.locs = &normalMapLocs_;
    data.winWidth = static_cast<float>(width_);
    data.winHeight = static_cast<float>(height_);

    for (const auto& [id, light] : data.lights) {
        if (light && light->enabled && data.enabledLightCount < MAX_LIGHTS) {
            data.enabledLightCount++;
        }
    }
    return data;
}

void LightingSystem::loadShaders_() {
    lightAccumShader_ = LoadShaderFromMemory(nullptr, lightAccumFragmentShader);
    normalMapShader_ = LoadShaderFromMemory(nullptr, normalMapFragmentShader);
    combineShader_ = LoadShaderFromMemory(nullptr, combineFragmentShader);
}

void LightingSystem::unloadShaders_() const {
    UnloadShader(lightAccumShader_);
    UnloadShader(normalMapShader_);
    UnloadShader(combineShader_);
}

void LightingSystem::createRenderTextures_(const int width, const int height) {
    lightBuffer_ = LoadRenderTexture(width, height);
}

void LightingSystem::destroyRenderTextures_() {
    if (lightBuffer_.texture.id != 0) {
        UnloadRenderTexture(lightBuffer_);
        lightBuffer_ = RenderTexture2D{};
    }
}

void LightingSystem::cacheUniformLocations_() {
    // Light accumulation shader
    lightAccumLoc_lightPos_ = GetShaderLocation(lightAccumShader_, "u_lightPos");
    lightAccumLoc_lightColor_ = GetShaderLocation(lightAccumShader_, "u_lightColor");
    lightAccumLoc_lightRadius_ = GetShaderLocation(lightAccumShader_, "u_lightRadius");
    lightAccumLoc_lightIntensity_ = GetShaderLocation(lightAccumShader_, "u_lightIntensity");
    lightAccumLoc_resolution_ = GetShaderLocation(lightAccumShader_, "u_resolution");

    // Normal map shader - cache array locations for each light index
    for (int i = 0; i < MAX_LIGHTS; i++) {
        char posName[32];
        snprintf(posName, sizeof(posName), "u_lightPos[%d]", i);
        normalMapLocs_.lightPos[i] = GetShaderLocation(normalMapShader_, posName);

        char colorName[32];
        snprintf(colorName, sizeof(colorName), "u_lightColor[%d]", i);
        normalMapLocs_.lightColor[i] = GetShaderLocation(normalMapShader_, colorName);

        char radiusName[32];
        snprintf(radiusName, sizeof(radiusName), "u_lightRadius[%d]", i);
        normalMapLocs_.lightRadius[i] = GetShaderLocation(normalMapShader_, radiusName);

        char intensityName[32];
        snprintf(intensityName, sizeof(intensityName), "u_lightIntensity[%d]", i);
        normalMapLocs_.lightIntensity[i] = GetShaderLocation(normalMapShader_, intensityName);
    }
    normalMapLocs_.lightCount = GetShaderLocation(normalMapShader_, "u_lightCount");
    normalMapLocs_.ambient = GetShaderLocation(normalMapShader_, "u_ambient");
    normalMapLocs_.resolution = GetShaderLocation(normalMapShader_, "u_resolution");
    normalMapLocs_.normalMap = GetShaderLocation(normalMapShader_, "u_normalMap");

    // Combine shader
    combineLoc_lightBuffer_ = GetShaderLocation(combineShader_, "u_lightBuffer");
    combineLoc_ambient_ = GetShaderLocation(combineShader_, "u_ambient");
}

} // namespace rlge
