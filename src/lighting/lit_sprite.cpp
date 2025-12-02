#include "lit_sprite.hpp"

#include "entity.hpp"
#include "scene.hpp"
#include "runtime.hpp"
#include "transformer.hpp"

#include <cstdio>

namespace rlge {

LitSprite::LitSprite(Entity& e, Texture2D& diffuse, Texture2D& normalMap,
                     int frameW, int frameH, LightingSystem& lighting)
    : Component(e)
    , diffuse_(diffuse)
    , normalMap_(normalMap)
    , fw_(frameW)
    , fh_(frameH)
    , layer_(InvalidLayerId)
    , lighting_(lighting) {}

LitSprite::LitSprite(Entity& e, Texture2D& diffuse, Texture2D& normalMap,
                     int frameW, int frameH, LightingSystem& lighting, LayerId layer)
    : Component(e)
    , diffuse_(diffuse)
    , normalMap_(normalMap)
    , fw_(frameW)
    , fh_(frameH)
    , layer_(layer)
    , lighting_(lighting) {}

void LitSprite::draw() {
    const auto* t = entity().get<Transform>();
    if (!t)
        return;

    const Rectangle src{
        0.0f,
        0.0f,
        static_cast<float>(fw_),
        static_cast<float>(fh_)
    };

    const Vector2 pos{t->position.x, t->position.y};
    const Vector2 scale{t->scale.x, t->scale.y};
    const Vector2 size{src.width * scale.x, src.height * scale.y};
    const Vector2 origin{size.x * 0.5f, size.y * 0.5f};
    const Rectangle dest{pos.x, pos.y, size.x, size.y};
    const float rotation = t->rotation;

    auto& scene = entity().scene();
    auto& rq = scene.rq();

    // Resolve layer: use provided layer or default to world
    LayerId effectiveLayer = layer_;
    if (effectiveLayer == InvalidLayerId) {
        effectiveLayer = scene.layers().world();
    }

    const Shader& shader = lighting_.normalMapShader();
    
    // Capture values (not references) for the draw call lambda to avoid dangling references
    const auto& lights = lighting_.lights();
    const auto& ambient = lighting_.ambient();
    const auto& locs = lighting_.normalMapLocations();
    const View* primaryView = scene.primaryView();
    Camera2D cam = primaryView && primaryView->camera ? primaryView->camera->cam2d() : Camera2D{};
    bool hasCamera = primaryView && primaryView->camera;
    float winWidth = static_cast<float>(lighting_.width());
    float winHeight = static_cast<float>(lighting_.height());
    
    // Count enabled lights
    int enabledLightCount = 0;
    for (size_t i = 0; i < lights.size() && enabledLightCount < MAX_LIGHTS; i++) {
        if (lights[i].enabled) enabledLightCount++;
    }
    
    rq.submitCustom(effectiveLayer, pos.y, shader, 
        [this, src, dest, origin, rotation, &lights, &ambient, &locs, cam, hasCamera, winWidth, winHeight, enabledLightCount]() {
            const Shader& normalShader = lighting_.normalMapShader();
            
            // Set resolution uniform
            Vector2 resolution = {winWidth, winHeight};
            SetShaderValue(normalShader, locs.resolution, &resolution, SHADER_UNIFORM_VEC2);
            
            // Set ambient
            Vector3 ambientVec = {
                ambient.color.r / 255.0f,
                ambient.color.g / 255.0f,
                ambient.color.b / 255.0f
            };
            SetShaderValue(normalShader, locs.ambient, &ambientVec, SHADER_UNIFORM_VEC3);
            
            // Set light count
            SetShaderValue(normalShader, locs.lightCount, &enabledLightCount, SHADER_UNIFORM_INT);
            
            // Set light arrays (convert world to screen positions)
            if (hasCamera) {
                int shaderLightIndex = 0;
                for (size_t i = 0; i < lights.size() && shaderLightIndex < MAX_LIGHTS; i++) {
                    const auto& light = lights[i];
                    if (!light.enabled) continue;
                    
                    // Convert world position to screen position
                    Vector2 screenPos = GetWorldToScreen2D(light.position, cam);
                    SetShaderValue(normalShader, locs.lightPos[shaderLightIndex], &screenPos, SHADER_UNIFORM_VEC2);
                    
                    Vector3 colorVec = {
                        light.color.r / 255.0f,
                        light.color.g / 255.0f,
                        light.color.b / 255.0f
                    };
                    SetShaderValue(normalShader, locs.lightColor[shaderLightIndex], &colorVec, SHADER_UNIFORM_VEC3);
                    
                    // Scale radius by camera zoom
                    float scaledRadius = light.radius * cam.zoom;
                    SetShaderValue(normalShader, locs.lightRadius[shaderLightIndex], &scaledRadius, SHADER_UNIFORM_FLOAT);
                    SetShaderValue(normalShader, locs.lightIntensity[shaderLightIndex], &light.intensity, SHADER_UNIFORM_FLOAT);
                    
                    shaderLightIndex++;
                }
            }
            
            // Bind normal map to texture slot 1
            SetShaderValueTexture(normalShader, locs.normalMap, normalMap_);
            
            // Draw the sprite
            DrawTexturePro(diffuse_, src, dest, origin, rotation, WHITE);
        });
}

} // namespace rlge
