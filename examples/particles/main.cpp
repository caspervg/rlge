#include "debug.hpp"
#include "runtime.hpp"
#include "window.hpp"
#include "particle_emitter.hpp"

#include "imgui.h"
#include "raylib.h"

using namespace rlge;

class FpsCounter final : public RenderEntity {
public:
    explicit FpsCounter(Scene& scene) :
        RenderEntity(scene) {}

    void draw() override {
        rq().submitUI([] {
            DrawFPS(10, 10);
        });
    }
};

class ParticleDemoScene final : public Scene, public HasDebugOverlay {
public:
    explicit ParticleDemoScene(Runtime& r) :
        Scene(r) {}

    void enter() override {
        ParticleEmitterConfig mouseCfg{
            .emitRate = 250.0f,
            .spread = 2.0f * PI,
            .gravity = {0.0f, 50.0f}
        };

        emitter_ = &spawn<ParticleEmitterEntity>(mouseCfg, [](const Particle& p) {
            DrawCircleV(p.pos, p.size, p.color);
        });

        constexpr auto halfWidth = 800.0f;
        constexpr auto topY = -450.0f;
        ParticleEmitterConfig rainCfg{
            .origin = {0.0f, topY},
            .emitRate = 800.0f,
            .minLifetime = 1.0f,
            .maxLifetime = 3.5f,
            .minSize = 4.0f,
            .maxSize = 6.0f,
            .gravity = {0.0f, 600.0f},
            .startColor = DARKBLUE,
            .endColor = Fade(SKYBLUE, 0.1f)
        };

        rainEmitter_ = &spawn<ParticleEmitterEntity>(rainCfg, [](const Particle& p) {
            // Simple raindrop: short line segment falling down.
            const Vector2 end{p.pos.x, p.pos.y + p.size * 2.0f};
            DrawLineV(p.pos, end, p.color);
        });

        fps_ = &spawn<FpsCounter>();

        if (!emitter_)
            return;

        // Emit around the origin within a small box when using the mouse-following origin.
        emitter_->setSpawnFn([](Vector2 origin) {
            return spawnInBox(origin, 30.0f, 30.0f);
        });

        if (rainEmitter_) {
            // Rain along a line near the top of the view, using a line spawn function.
            rainEmitter_->setSpawnFn([halfWidth, topY](Vector2) {
                const Vector2 a{-halfWidth, topY};
                const Vector2 b{halfWidth, topY};
                return spawnOnLine(a, b);
            });
        }
    }

    void update(float dt) override {
        // Move emitter origin with mouse in world space for a more interactive demo.
        if (emitter_) {
            const Vector2 mouse = GetMousePosition();
            const auto& cam = camera().camera();
            const Vector2 worldMouse = GetScreenToWorld2D(mouse, cam);
            emitter_->setOrigin(worldMouse);
        }

        Scene::update(dt);
    }

    void debugOverlay() override {
        if (!emitter_)
            return;

        ImGui::Begin("Particle Demo");

        float rate = emitter_->emitRate();
        if (ImGui::SliderFloat("Emit rate", &rate, 0.0f, 2000.0f)) {
            emitter_->setEmitRate(rate);
        }

        int maxParticles = static_cast<int>(emitter_->maxParticles());
        if (ImGui::SliderInt("Max particles", &maxParticles, 0, 5000)) {
            emitter_->setMaxParticles(static_cast<std::size_t>(maxParticles));
        }

        float minLife = emitter_->minLifetime();
        float maxLife = emitter_->maxLifetime();
        if (ImGui::DragFloatRange2("Lifetime", &minLife, &maxLife, 0.01f, 0.05f, 5.0f)) {
            emitter_->setLifetimeRange(minLife, maxLife);
        }

        float minSpeed = emitter_->minSpeed();
        float maxSpeed = emitter_->maxSpeed();
        if (ImGui::DragFloatRange2("Speed", &minSpeed, &maxSpeed, 1.0f, 0.0f, 1000.0f)) {
            emitter_->setSpeedRange(minSpeed, maxSpeed);
        }

        float minSize = emitter_->minSize();
        float maxSize = emitter_->maxSize();
        if (ImGui::DragFloatRange2("Size", &minSize, &maxSize, 0.1f, 0.1f, 100.0f)) {
            emitter_->setSizeRange(minSize, maxSize);
        }

        float spread = emitter_->spread();
        if (ImGui::SliderAngle("Spread", &spread, 0.0f, 360.0f)) {
            emitter_->setSpread(spread);
        }

        float direction = emitter_->direction();
        if (ImGui::SliderAngle("Direction", &direction, -180.0f, 180.0f)) {
            emitter_->setDirection(direction);
        }

        Color start = emitter_->startColor();
        float startCol[4] = {
            start.r / 255.0f,
            start.g / 255.0f,
            start.b / 255.0f,
            start.a / 255.0f
        };
        Color end = emitter_->endColor();
        float endCol[4] = {
            end.r / 255.0f,
            end.g / 255.0f,
            end.b / 255.0f,
            end.a / 255.0f
        };

        if (ImGui::ColorEdit4("Start color", startCol)) {
            Color newStart{
                static_cast<unsigned char>(startCol[0] * 255.0f),
                static_cast<unsigned char>(startCol[1] * 255.0f),
                static_cast<unsigned char>(startCol[2] * 255.0f),
                static_cast<unsigned char>(startCol[3] * 255.0f)
            };
            emitter_->setColorRange(newStart, end);
        }

        if (ImGui::ColorEdit4("End color", endCol)) {
            Color newEnd{
                static_cast<unsigned char>(endCol[0] * 255.0f),
                static_cast<unsigned char>(endCol[1] * 255.0f),
                static_cast<unsigned char>(endCol[2] * 255.0f),
                static_cast<unsigned char>(endCol[3] * 255.0f)
            };
            emitter_->setColorRange(start, newEnd);
        }

        Vector2 gravity = emitter_->gravity();
        if (ImGui::SliderFloat2("Gravity", &gravity.x, -1000.0f, 1000.0f)) {
            emitter_->setGravity(gravity);
        }

        ImGui::End();
    }

private:
    ParticleEmitterEntity* emitter_{nullptr};
    ParticleEmitterEntity* rainEmitter_{nullptr};
    FpsCounter* fps_{nullptr};
};

int main() {
    WindowConfig cfg{
        .width = 1600,
        .height = 900,
        .fps = 144,
        .title = "RLGE Particles"
    };
    Runtime runtime(cfg);

    runtime.pushScene<ParticleDemoScene>();
    runtime.run();

    return 0;
}
