#include <cmath>

#include "debug.hpp"
#include "imgui.h"
#include "render_entity.hpp"
#include "runtime.hpp"
#include "sprite.hpp"
#include "transformer.hpp"
#include "window.hpp"
#include "lighting/lit_scene.hpp"
#include "lighting/lit_sprite.hpp"

using namespace rlge;

// Entity with normal-mapped sprite
class LitBox final : public RenderEntity {
public:
    LitBox(Scene& scene, Texture2D& diffuse, Texture2D& normalMap, 
           LightingSystem& lighting, float x, float y)
        : RenderEntity(scene) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x, y};
        add<LitSprite>(diffuse, normalMap, diffuse.width, diffuse.height, lighting);
    }
};

// Regular sprite (not normal-mapped)
class RegularSprite final : public RenderEntity {
public:
    RegularSprite(Scene& scene, Texture2D& texture, float x, float y)
        : RenderEntity(scene) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x, y};
        add<Sprite>(texture, texture.width, texture.height);
    }
};

// Lit background sprite
class Background final : public RenderEntity {
public:
    Background(Scene& scene, Texture2D& diffuse, Texture2D& normalMap,
               LightingSystem& lighting, Vector2 pos)
        : RenderEntity(scene) {
        auto& tr = add<rlge::Transform>();
        tr.position = pos;
        add<LitSprite>(diffuse, normalMap, diffuse.width, diffuse.height, lighting,
                       scene.layers().background());
    }
};

// FPS counter
class FpsCounter final : public RenderEntity {
public:
    explicit FpsCounter(Scene& scene)
        : RenderEntity(scene) {}

    void draw() override {
        rq().submitUI([] {
            DrawFPS(10, 10);
        });
    }
};

class LightingDemoScene final : public LitScene, public HasDebugOverlay {
public:
    explicit LightingDemoScene(Runtime& r)
        : LitScene(r) {}

    void enter() override {
        LitScene::enter();

        // Create textures programmatically
        // Diffuse texture (blue box)
        Image boxImg = GenImageColor(64, 64, BLUE);
        ImageDrawRectangle(&boxImg, 4, 4, 56, 56, SKYBLUE);
        boxDiffuse_ = LoadTextureFromImage(boxImg);
        UnloadImage(boxImg);

        // Normal map (flat normals pointing up - suitable for 2D)
        Image normalImg = GenImageColor(64, 64, {128, 128, 255, 255}); // Flat normal (0, 0, 1)
        // Add some variation to make it more interesting
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                // Create slight bevel effect near edges
                float nx = 0.0f;
                float ny = 0.0f;
                float nz = 1.0f;
                
                int edgeDist = 8;
                if (x < edgeDist) {
                    nx = static_cast<float>(edgeDist - x) / edgeDist * 0.5f;
                } else if (x >= 64 - edgeDist) {
                    nx = -static_cast<float>(x - (64 - edgeDist)) / edgeDist * 0.5f;
                }
                if (y < edgeDist) {
                    ny = -static_cast<float>(edgeDist - y) / edgeDist * 0.5f;
                } else if (y >= 64 - edgeDist) {
                    ny = static_cast<float>(y - (64 - edgeDist)) / edgeDist * 0.5f;
                }
                
                // Normalize
                float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                nx /= len;
                ny /= len;
                nz /= len;
                
                // Convert from [-1,1] to [0,255]
                Color normalColor = {
                    static_cast<unsigned char>((nx * 0.5f + 0.5f) * 255.0f),
                    static_cast<unsigned char>((ny * 0.5f + 0.5f) * 255.0f),
                    static_cast<unsigned char>((nz * 0.5f + 0.5f) * 255.0f),
                    255
                };
                ImageDrawPixel(&normalImg, x, y, normalColor);
            }
        }
        boxNormal_ = LoadTextureFromImage(normalImg);
        UnloadImage(normalImg);

        // Plain sprite texture
        Image plainImg = GenImageColor(48, 48, GREEN);
        ImageDrawRectangle(&plainImg, 4, 4, 40, 40, LIME);
        plainTexture_ = LoadTextureFromImage(plainImg);
        UnloadImage(plainImg);

        const auto windowSize = runtime().window().size();

        // Background (tiled floor) diffuse/normal
        const int bgW = static_cast<int>(windowSize.x);
        const int bgH = static_cast<int>(windowSize.y);
        Image bgImg = GenImageColor(bgW, bgH, {28, 32, 40, 255});
        const int tile = 64;
        for (int y = 0; y < bgH; y += tile) {
            for (int x = 0; x < bgW; x += tile) {
                Color c = ((x / tile + y / tile) % 2 == 0)
                              ? Color{36, 42, 52, 255}
                              : Color{30, 36, 46, 255};
                ImageDrawRectangle(&bgImg, x, y, tile, tile, c);
            }
        }
        bgDiffuse_ = LoadTextureFromImage(bgImg);
        UnloadImage(bgImg);

        Image bgNormImg = GenImageColor(bgW, bgH, {128, 128, 255, 255}); // Flat normals
        bgNormal_ = LoadTextureFromImage(bgNormImg);
        UnloadImage(bgNormImg);

        // Set up camera
        camera_ = rlge::Camera();
        camera_.setOffset({windowSize.x * 0.5f, windowSize.y * 0.5f});
        camera_.setTarget({windowSize.x * 0.5f, windowSize.y * 0.5f});
        setSingleView(camera_);

        // Set darker ambient for dramatic effect
        lighting().setAmbient({20, 20, 30, 255});

        // Background covering the scene (lit)
        const auto win = runtime().window().size();
        spawn<Background>(bgDiffuse_, bgNormal_, lighting(), Vector2{win.x * 0.5f, win.y * 0.5f});

        // Add static lights
        staticLight1_ = lighting().addPointLight({150, 200}, 200.0f, RED, 0.8f);
        staticLight2_ = lighting().addPointLight({650, 200}, 200.0f, {50, 200, 255, 255}, 0.8f);
        
        // Add torch light (will flicker)
        torchLight_ = lighting().addPointLight({400, 400}, 180.0f, {255, 150, 50, 255}, 1.0f);
        
        // Add mouse-following light
        mouseLight_ = lighting().addPointLight({400, 300}, 150.0f, {255, 255, 200, 255}, 1.2f);

        // Spawn entities
        spawn<FpsCounter>();

        // Spawn normal-mapped sprites
        for (int i = 0; i < 5; i++) {
            spawn<LitBox>(boxDiffuse_, boxNormal_, lighting(), 
                          100.0f + i * 150.0f, 200.0f);
        }
        for (int i = 0; i < 3; i++) {
            spawn<LitBox>(boxDiffuse_, boxNormal_, lighting(), 
                          200.0f + i * 200.0f, 400.0f);
        }

        // Spawn regular sprites for comparison
        spawn<RegularSprite>(plainTexture_, 100.0f, 500.0f);
        spawn<RegularSprite>(plainTexture_, 200.0f, 500.0f);
    }

    void update(float dt) override {
        LitScene::update(dt);

        // Update mouse light position
        if (auto* mouseL = lighting().getLight(mouseLight_)) {
            Vector2 mouseWorld = camera_.mouseWorldPosition();
            mouseL->position = mouseWorld;
        }

        // Flicker the torch
        torchTimer_ += dt;
        if (auto* torch = lighting().getLight(torchLight_)) {
            // Use noise-like flickering
            float flicker = 0.8f + 0.2f * std::sin(torchTimer_ * 10.0f) 
                          + 0.1f * std::sin(torchTimer_ * 23.0f)
                          + 0.05f * std::sin(torchTimer_ * 47.0f);
            torch->intensity = flicker;
            
            // Also slightly vary the radius
            torch->radius = 180.0f + 10.0f * std::sin(torchTimer_ * 15.0f);
        }
    }

    void exit() override {
        UnloadTexture(boxDiffuse_);
        UnloadTexture(boxNormal_);
        UnloadTexture(plainTexture_);
        UnloadTexture(bgDiffuse_);
        UnloadTexture(bgNormal_);
    }

    void drawUnlit() override {
        // Draw instructions
        rq().submitUI([] {
            DrawText("Move mouse to control light", 10, 40, 20, WHITE);
            DrawText("Press F1 for debug panel", 10, 65, 20, WHITE);
        });
    }

    void debugOverlay() override {
        ImGui::Begin("Lighting Demo");

        // Ambient controls
        if (ImGui::CollapsingHeader("Ambient Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            Color ambient = lighting().ambient().color;
            float ambientColor[3] = {
                ambient.r / 255.0f,
                ambient.g / 255.0f,
                ambient.b / 255.0f
            };
            if (ImGui::ColorEdit3("Ambient Color", ambientColor)) {
                lighting().setAmbient({
                    static_cast<unsigned char>(ambientColor[0] * 255.0f),
                    static_cast<unsigned char>(ambientColor[1] * 255.0f),
                    static_cast<unsigned char>(ambientColor[2] * 255.0f),
                    255
                });
            }
        }

        // Light controls
        if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* lightNames[] = {"Red Light", "Blue Light", "Torch", "Mouse Light"};
            size_t lightIndices[] = {staticLight1_, staticLight2_, torchLight_, mouseLight_};

            for (int i = 0; i < 4; i++) {
                if (auto* light = lighting().getLight(lightIndices[i])) {
                    ImGui::PushID(i);
                    if (ImGui::TreeNode(lightNames[i])) {
                        ImGui::Checkbox("Enabled", &light->enabled);
                        ImGui::SliderFloat("Radius", &light->radius, 50.0f, 500.0f);
                        ImGui::SliderFloat("Intensity", &light->intensity, 0.0f, 2.0f);
                        
                        float color[3] = {
                            light->color.r / 255.0f,
                            light->color.g / 255.0f,
                            light->color.b / 255.0f
                        };
                        if (ImGui::ColorEdit3("Color", color)) {
                            light->color = {
                                static_cast<unsigned char>(color[0] * 255.0f),
                                static_cast<unsigned char>(color[1] * 255.0f),
                                static_cast<unsigned char>(color[2] * 255.0f),
                                255
                            };
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
        }

        // Render stats
        if (ImGui::CollapsingHeader("Render Stats")) {
            const auto& stats = rq().stats();
            ImGui::Text("Sprites: %zu", stats.spritesSubmitted);
            ImGui::Text("Batches: %zu", stats.batchCount);
            ImGui::Text("Draw Calls: %zu", stats.drawCalls);
            ImGui::Text("Custom Commands: %zu", stats.customCommands);
        }

        ImGui::End();
    }

private:
    rlge::Camera camera_;
    Texture2D boxDiffuse_{};
    Texture2D boxNormal_{};
    Texture2D plainTexture_{};
    Texture2D bgDiffuse_{};
    Texture2D bgNormal_{};

    size_t staticLight1_{0};
    size_t staticLight2_{0};
    size_t torchLight_{0};
    size_t mouseLight_{0};

    float torchTimer_{0.0f};
};

int main() {
    Runtime runtime(WindowConfig{
        .width = 1920,
        .height = 1080,
        .fps = 144,
        .title = "RLGE Lighting Demo"
    });

    runtime.pushScene<LightingDemoScene>();
    runtime.run();

    return 0;
}
