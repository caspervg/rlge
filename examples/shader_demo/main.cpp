// Shader Demo - Demonstrates the RLGE shader system
// Features:
// - Per-layer shaders (water wave effect on a custom layer)
// - Per-entity shaders (hit flash effect on individual sprites)
// - Dynamic layer creation and management
// - ShaderParams<T> typed uniform binding

#include <print>
#include <cmath>

#include "debug.hpp"
#include "runtime.hpp"
#include "window.hpp"
#include "imgui.h"
#include "sprite.hpp"
#include "transformer.hpp"
#include "render_entity.hpp"
#include "shader_effect.hpp"

using namespace rlge;

// Shader uniform parameters for wave effect
struct WaveParams {
    float time = 0.0f;
    float amplitude = 0.02f;
    float frequency = 10.0f;
    float speed = 3.0f;
};

// Shader uniform parameters for flash effect
struct FlashParams {
    float intensity = 0.0f;
    Vector3 flashColor = {1.0f, 1.0f, 1.0f};
};

// Fragment shader source for wave distortion effect
static const char* waveFragmentShader = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float u_time;
uniform float u_amplitude;
uniform float u_frequency;
uniform float u_speed;

out vec4 finalColor;

void main() {
    vec2 uv = fragTexCoord;
    uv.x += sin(uv.y * u_frequency + u_time * u_speed) * u_amplitude;
    uv.y += cos(uv.x * u_frequency + u_time * u_speed) * u_amplitude * 0.5;
    finalColor = texture(texture0, uv) * fragColor;
}
)";

// Fragment shader source for hit flash effect
static const char* flashFragmentShader = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float u_intensity;
uniform vec3 u_flashColor;

out vec4 finalColor;

void main() {
    vec4 texColor = texture(texture0, fragTexCoord) * fragColor;
    vec3 flash = mix(texColor.rgb, u_flashColor, u_intensity);
    finalColor = vec4(flash, texColor.a);
}
)";

class FpsCounter final : public RenderEntity {
public:
    explicit FpsCounter(Scene& scene) : RenderEntity(scene) {}

    void draw() override {
        rq().submitUI([] {
            DrawFPS(10, 10);
        });
    }
};

// Entity on the wave layer (affected by wave shader)
class WaveSprite final : public RenderEntity {
public:
    WaveSprite(Scene& scene, Texture2D& texture, LayerId layer, float x, float y)
        : RenderEntity(scene)
        , texture_(texture)
        , layer_(layer) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x, y};
        add<Sprite>(texture, texture.width, texture.height, layer);
    }

private:
    Texture2D& texture_;
    LayerId layer_;
};

// Entity with per-entity flash shader effect
class FlashingSprite final : public RenderEntity {
public:
    FlashingSprite(Scene& scene, Texture2D& texture, Shader flashShader, float x, float y)
        : RenderEntity(scene)
        , flashShader_(flashShader) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x, y};

        // Add per-entity shader effect
        add<ShaderEffect<FlashParams>>(flashShader)
            .bind("u_intensity", &FlashParams::intensity)
            .bind("u_flashColor", &FlashParams::flashColor);

        add<Sprite>(texture, texture.width, texture.height);
    }

    void update(float dt) override {
        RenderEntity::update(dt);

        // Pulse the flash effect
        flashTimer_ += dt * flashSpeed_;
        auto* effect = get<ShaderEffect<FlashParams>>();
        if (effect) {
            // Smooth pulse between 0 and 1
            effect->params().intensity = (std::sin(flashTimer_) + 1.0f) * 0.5f * maxIntensity_;
        }

        // Handle input for movement
        const auto& input = scene().input();
        auto* tr = get<rlge::Transform>();
        if (tr) {
            if (input.down(Action::MoveLeft))
                tr->position.x -= speed_ * dt;
            if (input.down(Action::MoveRight))
                tr->position.x += speed_ * dt;
            if (input.down(Action::MoveUp))
                tr->position.y -= speed_ * dt;
            if (input.down(Action::MoveDown))
                tr->position.y += speed_ * dt;
        }
    }

    float flashSpeed_ = 5.0f;
    float maxIntensity_ = 0.8f;
    float speed_ = 200.0f;

private:
    Shader flashShader_;
    float flashTimer_ = 0.0f;
};

// Normal sprite without shader (for comparison)
class NormalSprite final : public RenderEntity {
public:
    NormalSprite(Scene& scene, Texture2D& texture, float x, float y)
        : RenderEntity(scene) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x, y};
        add<Sprite>(texture, texture.width, texture.height);
    }
};

class ShaderDemoScene final : public Scene, public HasDebugOverlay {
public:
    explicit ShaderDemoScene(Runtime& r)
        : Scene(r) {}

    void enter() override {
        // Create textures programmatically
        Image boxImg = GenImageColor(64, 64, BLUE);
        ImageDrawRectangle(&boxImg, 4, 4, 56, 56, SKYBLUE);
        boxTexture_ = LoadTextureFromImage(boxImg);
        UnloadImage(boxImg);

        Image playerImg = GenImageColor(48, 48, RED);
        ImageDrawRectangle(&playerImg, 4, 4, 40, 40, ORANGE);
        playerTexture_ = LoadTextureFromImage(playerImg);
        UnloadImage(playerImg);

        // Load shaders
        waveShader_ = LoadShaderFromMemory(nullptr, waveFragmentShader);
        flashShader_ = LoadShaderFromMemory(nullptr, flashFragmentShader);

        // Create a custom "water" layer with the wave shader
        waterLayer_ = layers().create("water", 25);

        // Set up typed shader params for the water layer
        waveParams_ = ShaderParams<WaveParams>(waveShader_);
        waveParams_.bind("u_time", &WaveParams::time)
                   .bind("u_amplitude", &WaveParams::amplitude)
                   .bind("u_frequency", &WaveParams::frequency)
                   .bind("u_speed", &WaveParams::speed);

        layers().setShaderParams(waterLayer_, std::move(waveParams_));

        // Set up camera
        camera_ = rlge::Camera();
        camera_.setTarget({400, 300});
        setSingleView(camera_);

        // Spawn entities
        fps_ = &spawn<FpsCounter>();

        // Spawn some sprites on the wave layer
        for (int i = 0; i < 5; ++i) {
            waveSprites_.push_back(&spawn<WaveSprite>(boxTexture_, waterLayer_,
                                                       100.0f + i * 150.0f, 150.0f));
        }

        // Spawn flashing sprite (per-entity shader)
        flashingSprite_ = &spawn<FlashingSprite>(playerTexture_, flashShader_, 400.0f, 350.0f);

        // Spawn normal sprites for comparison
        normalSprites_.push_back(&spawn<NormalSprite>(boxTexture_, 100.0f, 400.0f));
        normalSprites_.push_back(&spawn<NormalSprite>(boxTexture_, 250.0f, 400.0f));
    }

    void update(float dt) override {
        Scene::update(dt);

        // Update wave shader time
        if (auto layer = layers().get(waterLayer_)) {
            if (layer->get().shaderParams) {
                // We need to access the typed params to update time
                // Since we moved the params, we need to get them from the layer
                auto* wrapper = dynamic_cast<ShaderParamsWrapper<WaveParams>*>(
                    layer->get().shaderParams.get());
                if (wrapper) {
                    wrapper->get().params().time += dt;
                }
            }
        }
    }

    void exit() override {
        UnloadShader(waveShader_);
        UnloadShader(flashShader_);
        UnloadTexture(boxTexture_);
        UnloadTexture(playerTexture_);
    }

    void debugOverlay() override {
        ImGui::Begin("Shader Demo");

        ImGui::Text("This demo shows the RLGE shader system:");
        ImGui::BulletText("Top row: Wave layer shader");
        ImGui::BulletText("Middle: Per-entity flash shader");
        ImGui::BulletText("Bottom row: Normal (no shader)");
        ImGui::Separator();

        ImGui::Text("Controls:");
        ImGui::BulletText("Arrow keys / WASD: Move flashing sprite");
        ImGui::BulletText("F1: Toggle this debug panel");
        ImGui::Separator();

        // Wave shader controls
        if (ImGui::CollapsingHeader("Wave Layer Shader", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (auto layer = layers().get(waterLayer_)) {
                auto* wrapper = dynamic_cast<ShaderParamsWrapper<WaveParams>*>(
                    layer->get().shaderParams.get());
                if (wrapper) {
                    auto& params = wrapper->get().params();
                    ImGui::SliderFloat("Amplitude", &params.amplitude, 0.0f, 0.1f);
                    ImGui::SliderFloat("Frequency", &params.frequency, 1.0f, 30.0f);
                    ImGui::SliderFloat("Speed", &params.speed, 0.5f, 10.0f);
                }
            }
        }

        // Flash shader controls
        if (ImGui::CollapsingHeader("Flash Entity Shader", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (flashingSprite_) {
                ImGui::SliderFloat("Flash Speed", &flashingSprite_->flashSpeed_, 0.5f, 15.0f);
                ImGui::SliderFloat("Max Intensity", &flashingSprite_->maxIntensity_, 0.0f, 1.0f);

                auto* effect = flashingSprite_->get<ShaderEffect<FlashParams>>();
                if (effect) {
                    ImGui::ColorEdit3("Flash Color",
                        reinterpret_cast<float*>(&effect->params().flashColor));
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
    Texture2D boxTexture_{};
    Texture2D playerTexture_{};
    Shader waveShader_{};
    Shader flashShader_{};
    LayerId waterLayer_ = InvalidLayerId;
    ShaderParams<WaveParams> waveParams_{Shader{0}};

    FpsCounter* fps_ = nullptr;
    std::vector<WaveSprite*> waveSprites_;
    FlashingSprite* flashingSprite_ = nullptr;
    std::vector<NormalSprite*> normalSprites_;
};

int main() {
    WindowConfig cfg{
        .width = 800,
        .height = 600,
        .fps = 60,
        .title = "RLGE Shader Demo"
    };

    Runtime runtime(cfg);

    // Input bindings
    runtime.input().bind(Action::MoveLeft, KeyCode::Left);
    runtime.input().bind(Action::MoveLeft, KeyCode::A);
    runtime.input().bind(Action::MoveRight, KeyCode::Right);
    runtime.input().bind(Action::MoveRight, KeyCode::D);
    runtime.input().bind(Action::MoveUp, KeyCode::Up);
    runtime.input().bind(Action::MoveUp, KeyCode::W);
    runtime.input().bind(Action::MoveDown, KeyCode::Down);
    runtime.input().bind(Action::MoveDown, KeyCode::S);

    runtime.pushScene<ShaderDemoScene>();
    runtime.run();

    return 0;
}
