#include "lit_sprite.hpp"

#include "entity.hpp"
#include "runtime.hpp"
#include "scene.hpp"
#include "transformer.hpp"

namespace rlge {

LitSprite::LitSprite(Entity& e, Texture2D& diffuse, Texture2D& normalMap,
                     const int frameW, const int frameH, LightingSystem& lighting)
    : Component(e)
    , diffuse_(diffuse)
    , normalMap_(normalMap)
    , fw_(frameW)
    , fh_(frameH)
    , layer_(InvalidLayerId)
    , lighting_(lighting) {}

LitSprite::LitSprite(Entity& e, Texture2D& diffuse, Texture2D& normalMap,
                     const int frameW, const int frameH, LightingSystem& lighting, const LayerId layer)
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
    
    auto passData = lighting_.normalPassData();
    
    rq.submitCustom(effectiveLayer, pos.y, shader, 
        [this, src, dest, origin, rotation, &rq, passData = std::move(passData)]() {
            const Shader& normalShader = lighting_.normalMapShader();
            const auto* viewCtx = rq.currentView();

            // If there is no active view/camera, fall back to unlit sprite draw
            if (!viewCtx || !viewCtx->camera) {
                constexpr auto zeroLights = 0;
                const Vector2 resolution = {passData.winWidth, passData.winHeight};
                SetShaderValue(normalShader, passData.locs->resolution, &resolution, SHADER_UNIFORM_VEC2);
                Vector3 ambientVec = {
                    passData.ambient.color.r / 255.0f,
                    passData.ambient.color.g / 255.0f,
                    passData.ambient.color.b / 255.0f
                };
                SetShaderValue(normalShader, passData.locs->ambient, &ambientVec, SHADER_UNIFORM_VEC3);
                SetShaderValue(normalShader, passData.locs->lightCount, &zeroLights, SHADER_UNIFORM_INT);
                SetShaderValueTexture(normalShader, passData.locs->normalMap, normalMap_);
                DrawTexturePro(diffuse_, src, dest, origin, rotation, WHITE);
                return;
            }

            const Camera2D& cam = *viewCtx->camera;
            
            // Set resolution uniform
            Vector2 resolution = {passData.winWidth, passData.winHeight};
            SetShaderValue(normalShader, passData.locs->resolution, &resolution, SHADER_UNIFORM_VEC2);
            
            // Set ambient
            const Vector3 ambientVec = {
                passData.ambient.color.r / 255.0f,
                passData.ambient.color.g / 255.0f,
                passData.ambient.color.b / 255.0f
            };
            SetShaderValue(normalShader, passData.locs->ambient, &ambientVec, SHADER_UNIFORM_VEC3);
            
            // Set light count
            SetShaderValue(normalShader, passData.locs->lightCount, &passData.enabledLightCount, SHADER_UNIFORM_INT);
            
            // Set light arrays (convert world to screen positions)
            auto shaderLightIndex = 0;
            for (const auto& entry : passData.lights) {
                if (shaderLightIndex >= MAX_LIGHTS) break;
                if (!entry.light || !entry.light->enabled) continue;
                const auto& light = *entry.light;
                
                // Convert world position to screen position for the active view
                Vector2 screenPos = GetWorldToScreen2D(light.position, cam);
                SetShaderValue(normalShader, passData.locs->lightPos[shaderLightIndex], &screenPos, SHADER_UNIFORM_VEC2);
                
                Vector3 colorVec = {
                    light.color.r / 255.0f,
                    light.color.g / 255.0f,
                    light.color.b / 255.0f
                };
                SetShaderValue(normalShader, passData.locs->lightColor[shaderLightIndex], &colorVec, SHADER_UNIFORM_VEC3);
                
                // Scale radius by camera zoom
                float scaledRadius = light.radius * cam.zoom;
                SetShaderValue(normalShader, passData.locs->lightRadius[shaderLightIndex], &scaledRadius, SHADER_UNIFORM_FLOAT);
                SetShaderValue(normalShader, passData.locs->lightIntensity[shaderLightIndex], &light.intensity, SHADER_UNIFORM_FLOAT);
                
                shaderLightIndex++;
            }
            
            // Bind a normal map to texture slot 1
            SetShaderValueTexture(normalShader, passData.locs->normalMap, normalMap_);
            
            // Draw the sprite
            DrawTexturePro(diffuse_, src, dest, origin, rotation, WHITE);
        });
}

} // namespace rlge
