#include "lighting_system.hpp"

#include <algorithm>

namespace rlge {

// Light accumulation fragment shader - renders radial gradient lights
static const char* lightAccumFragmentShader = R"(
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
    float dist = distance(screenPos, u_lightPos);
    
    // Smooth falloff
    float attenuation = 1.0 - smoothstep(0.0, u_lightRadius, dist);
    attenuation = attenuation * attenuation; // Quadratic falloff
    
    vec3 light = u_lightColor * attenuation * u_lightIntensity;
    finalColor = vec4(light, 1.0);
}
)";

// Normal map fragment shader - per-pixel lighting using normal map
static const char* normalMapFragmentShader = R"(
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
    
    vec2 fragPos = gl_FragCoord.xy;
    
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

// Combine fragment shader - multiplies scene by (ambient + lightBuffer)
static const char* combineFragmentShader = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;      // Scene texture
uniform sampler2D u_lightBuffer; // Light accumulation buffer
uniform vec3 u_ambient;

out vec4 finalColor;

void main() {
    vec4 sceneColor = texture(texture0, fragTexCoord);
    vec3 lightColor = texture(u_lightBuffer, fragTexCoord).rgb;
    
    // Combine scene with lighting (ambient + light buffer)
    vec3 totalLight = u_ambient + lightColor;
    
    // Clamp to avoid over-brightening
    totalLight = clamp(totalLight, 0.0, 2.0);
    
    finalColor = vec4(sceneColor.rgb * totalLight, sceneColor.a);
}
)";

LightingSystem::~LightingSystem() {
    if (initialized_) {
        destroyRenderTextures();
        unloadShaders();
    }
}

void LightingSystem::init(int width, int height) {
    if (initialized_) {
        resize(width, height);
        return;
    }

    width_ = width;
    height_ = height;

    loadShaders();
    cacheUniformLocations();
    createRenderTextures(width, height);

    initialized_ = true;
}

void LightingSystem::resize(int width, int height) {
    if (width_ == width && height_ == height) {
        return;
    }

    width_ = width;
    height_ = height;

    destroyRenderTextures();
    createRenderTextures(width, height);
}

size_t LightingSystem::addPointLight(Vector2 pos, float radius, Color color, float intensity) {
    lights_.push_back({pos, color, radius, intensity, true});
    return lights_.size() - 1;
}

PointLight* LightingSystem::getLight(size_t index) {
    if (index < lights_.size()) {
        return &lights_[index];
    }
    return nullptr;
}

void LightingSystem::removeLight(size_t index) {
    if (index < lights_.size()) {
        lights_.erase(lights_.begin() + static_cast<long>(index));
    }
}

void LightingSystem::clearLights() {
    lights_.clear();
}

void LightingSystem::setAmbient(Color color) {
    ambient_.color = color;
}

void LightingSystem::beginFrame() {
    if (!initialized_) return;

    // Clear light buffer to black
    BeginTextureMode(lightBuffer_);
    ClearBackground(BLACK);
    EndTextureMode();
}

void LightingSystem::renderLights(const Camera2D& camera) {
    if (!initialized_) return;

    BeginTextureMode(lightBuffer_);
    BeginBlendMode(BLEND_ADDITIVE);

    // Set resolution uniform
    Vector2 resolution = {static_cast<float>(width_), static_cast<float>(height_)};
    SetShaderValue(lightAccumShader_, lightAccumLoc_resolution_, &resolution, SHADER_UNIFORM_VEC2);

    for (const auto& light : lights_) {
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

        // Draw fullscreen quad with shader
        BeginShaderMode(lightAccumShader_);
        DrawRectangle(0, 0, width_, height_, WHITE);
        EndShaderMode();
    }

    EndBlendMode();
    EndTextureMode();
}

void LightingSystem::applyLighting(Texture2D sceneTexture) {
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
    Rectangle srcRect = {
        0.0f, 0.0f,
        static_cast<float>(sceneTexture.width),
        -static_cast<float>(sceneTexture.height)
    };
    Rectangle destRect = {
        0.0f, 0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_)
    };
    
    DrawTexturePro(sceneTexture, srcRect, destRect, {0, 0}, 0.0f, WHITE);
    
    EndShaderMode();
}

void LightingSystem::loadShaders() {
    lightAccumShader_ = LoadShaderFromMemory(nullptr, lightAccumFragmentShader);
    normalMapShader_ = LoadShaderFromMemory(nullptr, normalMapFragmentShader);
    combineShader_ = LoadShaderFromMemory(nullptr, combineFragmentShader);
}

void LightingSystem::unloadShaders() {
    UnloadShader(lightAccumShader_);
    UnloadShader(normalMapShader_);
    UnloadShader(combineShader_);
}

void LightingSystem::createRenderTextures(int width, int height) {
    lightBuffer_ = LoadRenderTexture(width, height);
}

void LightingSystem::destroyRenderTextures() {
    UnloadRenderTexture(lightBuffer_);
}

void LightingSystem::cacheUniformLocations() {
    // Light accumulation shader
    lightAccumLoc_lightPos_ = GetShaderLocation(lightAccumShader_, "u_lightPos");
    lightAccumLoc_lightColor_ = GetShaderLocation(lightAccumShader_, "u_lightColor");
    lightAccumLoc_lightRadius_ = GetShaderLocation(lightAccumShader_, "u_lightRadius");
    lightAccumLoc_lightIntensity_ = GetShaderLocation(lightAccumShader_, "u_lightIntensity");
    lightAccumLoc_resolution_ = GetShaderLocation(lightAccumShader_, "u_resolution");

    // Normal map shader
    normalMapLoc_lightPos_ = GetShaderLocation(normalMapShader_, "u_lightPos");
    normalMapLoc_lightColor_ = GetShaderLocation(normalMapShader_, "u_lightColor");
    normalMapLoc_lightRadius_ = GetShaderLocation(normalMapShader_, "u_lightRadius");
    normalMapLoc_lightIntensity_ = GetShaderLocation(normalMapShader_, "u_lightIntensity");
    normalMapLoc_lightCount_ = GetShaderLocation(normalMapShader_, "u_lightCount");
    normalMapLoc_ambient_ = GetShaderLocation(normalMapShader_, "u_ambient");
    normalMapLoc_resolution_ = GetShaderLocation(normalMapShader_, "u_resolution");
    normalMapLoc_normalMap_ = GetShaderLocation(normalMapShader_, "u_normalMap");

    // Combine shader
    combineLoc_lightBuffer_ = GetShaderLocation(combineShader_, "u_lightBuffer");
    combineLoc_ambient_ = GetShaderLocation(combineShader_, "u_ambient");
}

} // namespace rlge
