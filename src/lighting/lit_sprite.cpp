#include "lit_sprite.hpp"

#include "entity.hpp"
#include "scene.hpp"
#include "runtime.hpp"
#include "transformer.hpp"

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

    Shader shader = lighting_.normalMapShader();
    
    // Capture needed data for the draw call
    rq.submitCustom(effectiveLayer, pos.y, shader, 
        [this, &scene, src, dest, origin, rotation]() {
            Shader normalShader = lighting_.normalMapShader();
            
            // Get the light data
            const auto& lights = lighting_.lights();
            const auto& ambient = lighting_.ambient();
            
            // Set resolution uniform
            auto [winWidth, winHeight] = scene.runtime().window().size();
            Vector2 resolution = {winWidth, winHeight};
            int resLoc = GetShaderLocation(normalShader, "u_resolution");
            SetShaderValue(normalShader, resLoc, &resolution, SHADER_UNIFORM_VEC2);
            
            // Set ambient
            Vector3 ambientVec = {
                ambient.color.r / 255.0f,
                ambient.color.g / 255.0f,
                ambient.color.b / 255.0f
            };
            int ambientLoc = GetShaderLocation(normalShader, "u_ambient");
            SetShaderValue(normalShader, ambientLoc, &ambientVec, SHADER_UNIFORM_VEC3);
            
            // Set light count
            int lightCount = static_cast<int>(std::min(lights.size(), static_cast<size_t>(MAX_LIGHTS)));
            int lightCountLoc = GetShaderLocation(normalShader, "u_lightCount");
            SetShaderValue(normalShader, lightCountLoc, &lightCount, SHADER_UNIFORM_INT);
            
            // Set light arrays (convert world to screen positions)
            const View* primaryView = scene.primaryView();
            if (primaryView && primaryView->camera) {
                const Camera2D& cam = primaryView->camera->cam2d();
                
                for (int i = 0; i < lightCount; i++) {
                    const auto& light = lights[i];
                    if (!light.enabled) continue;
                    
                    // Convert world position to screen position
                    Vector2 screenPos = GetWorldToScreen2D(light.position, cam);
                    
                    char posName[32];
                    snprintf(posName, sizeof(posName), "u_lightPos[%d]", i);
                    int posLoc = GetShaderLocation(normalShader, posName);
                    SetShaderValue(normalShader, posLoc, &screenPos, SHADER_UNIFORM_VEC2);
                    
                    Vector3 colorVec = {
                        light.color.r / 255.0f,
                        light.color.g / 255.0f,
                        light.color.b / 255.0f
                    };
                    char colorName[32];
                    snprintf(colorName, sizeof(colorName), "u_lightColor[%d]", i);
                    int colorLoc = GetShaderLocation(normalShader, colorName);
                    SetShaderValue(normalShader, colorLoc, &colorVec, SHADER_UNIFORM_VEC3);
                    
                    // Scale radius by camera zoom
                    float scaledRadius = light.radius * cam.zoom;
                    char radiusName[32];
                    snprintf(radiusName, sizeof(radiusName), "u_lightRadius[%d]", i);
                    int radiusLoc = GetShaderLocation(normalShader, radiusName);
                    SetShaderValue(normalShader, radiusLoc, &scaledRadius, SHADER_UNIFORM_FLOAT);
                    
                    char intensityName[32];
                    snprintf(intensityName, sizeof(intensityName), "u_lightIntensity[%d]", i);
                    int intensityLoc = GetShaderLocation(normalShader, intensityName);
                    SetShaderValue(normalShader, intensityLoc, &light.intensity, SHADER_UNIFORM_FLOAT);
                }
            }
            
            // Bind normal map to texture slot 1
            int normalMapLoc = GetShaderLocation(normalShader, "u_normalMap");
            SetShaderValueTexture(normalShader, normalMapLoc, normalMap_);
            
            // Draw the sprite
            DrawTexturePro(diffuse_, src, dest, origin, rotation, WHITE);
        });
}

} // namespace rlge
