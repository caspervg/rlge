#include <algorithm>
#include <utility>

#include "debug.hpp"
#include "particle_emitter.hpp"
#include "runtime.hpp"
#include "transformer.hpp"
#include "window.hpp"

#include "imgui.h"
#include "particle_fx.hpp"
#include "raylib.h"
#include "raymath.h"

using namespace rlge;

class ParticleEmitterEntity final : public RenderEntity {
public:
    ParticleEmitterEntity(Scene& scene, ContinuousEmitterConfig cfg, ContinuousParticleEmitter::RenderFn renderFn)
        : RenderEntity(scene)
        , emitter(add<ContinuousParticleEmitter>(cfg, std::move(renderFn))) {
        add<rlge::Transform>();
    }

    ContinuousParticleEmitter& emitter;
};

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
        ContinuousEmitterConfig mouseCfg{
            .emitRate = 250.0f,
            .spread = 2.0f * PI,
            .gravity = {0.0f, 50.0f}
        };
        mouseConfig_ = mouseCfg;

        camera_ = rlge::Camera();
        setSingleView(camera_);

        emitterEntity_ = &spawn<ParticleEmitterEntity>(mouseCfg, [this](const Particle& p) {
            renderParticle(p, streaksEnabled_);
        });
        emitter_ = emitterEntity_ ? &emitterEntity_->emitter : nullptr;

        ContinuousEmitterConfig rainCfg{
            .localOffset = {0.0f, 0.0f},
            .emitRate = 800.0f,
            .minLifetime = 1.0f,
            .maxLifetime = 3.5f,
            .minSize = 4.0f,
            .maxSize = 6.0f,
            .gravity = {0.0f, 600.0f},
            .startColor = DARKBLUE,
            .endColor = Fade(SKYBLUE, 0.1f)
        };

        rainEmitterEntity_ = &spawn<ParticleEmitterEntity>(rainCfg, [](const Particle& p) {
            // Simple raindrop: short line segment falling down.
            const Vector2 end{p.pos.x, p.pos.y + p.size * 2.0f};
            DrawLineV(p.pos, end, p.color);
        });
        rainEmitter_ = rainEmitterEntity_ ? &rainEmitterEntity_->emitter : nullptr;

        fps_ = &spawn<FpsCounter>();

        if (!emitter_)
            return;

        // Emit around the origin within a small box when using the mouse-following origin.
        emitter_->setSpawnFn([](const Vector2 origin) {
            return spawnInBox(origin, 30.0f, 30.0f);
        });

        if (rainEmitter_) {
            // Rain along a line near the top of the view, using a line spawn function.
            rainEmitter_->setSpawnFn([](Vector2) {
                const Vector2 a{0, 0};
                const Vector2 b{1600, 0};
                return spawnOnLine(a, b);
            });
        }
    }

    void update(const float dt) override {
        // Move emitter origin with mouse in world space for a more interactive demo.
        if (emitterEntity_) {
            const Vector2 mouse = GetMousePosition();
            const auto& cam = camera_.cam2d();
            const Vector2 worldMouse = GetScreenToWorld2D(mouse, cam);
            if (auto* t = emitterEntity_->get<rlge::Transform>()) {
                t->position = worldMouse;
            }
        }

        // Burst on click for a one-shot effect at the mouse position using a burst emitter helper.
        if (emitterEntity_ && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            BurstEmitterConfig burstCfg;
            burstCfg.maxParticles = static_cast<std::size_t>(burstCount_ * 2);
            burstCfg.minLifetime = emitter_->minLifetime();
            burstCfg.maxLifetime = emitter_->maxLifetime();
            burstCfg.minSpeed = emitter_->minSpeed();
            burstCfg.maxSpeed = emitter_->maxSpeed();
            burstCfg.minSize = emitter_->minSize();
            burstCfg.maxSize = emitter_->maxSize();
            burstCfg.spread = emitter_->spread();
            burstCfg.direction = emitter_->direction();
            burstCfg.gravity = emitter_->gravity();
            burstCfg.startColor = emitter_->startColor();
            burstCfg.endColor = emitter_->endColor();
            spawnBurstEmitter(
                *this,
                emitterEntity_->get<rlge::Transform>()->position,
                burstCount_,
                burstCfg,
                [this](const Particle& p) { renderParticle(p, streaksEnabled_); },
                emitter_->spawnFn());
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

        auto maxParticles = static_cast<int>(emitter_->maxParticles());
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
            const Color newStart{
                static_cast<unsigned char>(startCol[0] * 255.0f),
                static_cast<unsigned char>(startCol[1] * 255.0f),
                static_cast<unsigned char>(startCol[2] * 255.0f),
                static_cast<unsigned char>(startCol[3] * 255.0f)
            };
            emitter_->setColorRange(newStart, end);
        }

        if (ImGui::ColorEdit4("End color", endCol)) {
            const Color newEnd{
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

        ImGui::Checkbox("Velocity streaks", &streaksEnabled_);

        ImGui::SliderInt("Burst count", &burstCount_, 0, 5000);

        ImGui::Separator();
        if (ImGui::Button("Preset: Smoke")) {
            applyPresetSmoke();
        }
        ImGui::SameLine();
        if (ImGui::Button("Preset: Fireworks")) {
            applyPresetFireworks();
        }
        ImGui::SameLine();
        if (ImGui::Button("Preset: Sparks")) {
            applyPresetSparks();
        }

        ImGui::End();
    }

private:
    void renderParticle(const Particle& p, bool streak) {
        if (!streak) {
            DrawCircleV(p.pos, p.size, p.color);
            return;
        }
        const float speed = Vector2Length(p.vel);
        if (speed < 1e-3f) {
            DrawCircleV(p.pos, p.size, p.color);
            return;
        }
        const float len = std::max(4.0f, speed * 0.02f);
        const Vector2 dir = Vector2Normalize(p.vel);
        const Vector2 tail{p.pos.x - dir.x * len, p.pos.y - dir.y * len};
        DrawLineEx(tail, p.pos, p.size * 0.8f, p.color);
    }

    void applyConfigToEmitter(const ContinuousEmitterConfig& cfg) {
        if (!emitter_)
            return;
        emitter_->setEmitRate(cfg.emitRate);
        emitter_->setMaxParticles(cfg.maxParticles);
        emitter_->setLifetimeRange(cfg.minLifetime, cfg.maxLifetime);
        emitter_->setSpeedRange(cfg.minSpeed, cfg.maxSpeed);
        emitter_->setSizeRange(cfg.minSize, cfg.maxSize);
        emitter_->setSpread(cfg.spread);
        emitter_->setDirection(cfg.direction);
        emitter_->setGravity(cfg.gravity);
        emitter_->setColorRange(cfg.startColor, cfg.endColor);
    }

    void applyPresetSmoke() {
        ContinuousEmitterConfig cfg = mouseConfig_;
        cfg.emitRate = 180.0f;
        cfg.minLifetime = 0.8f;
        cfg.maxLifetime = 1.6f;
        cfg.minSpeed = 10.0f;
        cfg.maxSpeed = 40.0f;
        cfg.minSize = 8.0f;
        cfg.maxSize = 18.0f;
        cfg.spread = PI;
        cfg.direction = -PI * 0.5f;
        cfg.gravity = {0.0f, -10.0f};
        cfg.startColor = {160, 160, 160, 200};
        cfg.endColor = {90, 90, 90, 10};
        applyConfigToEmitter(cfg);
        mouseConfig_ = cfg;
        streaksEnabled_ = false;
    }

    void applyPresetFireworks() {
        ContinuousEmitterConfig cfg = mouseConfig_;
        cfg.emitRate = 400.0f;
        cfg.minLifetime = 0.6f;
        cfg.maxLifetime = 1.2f;
        cfg.minSpeed = 200.0f;
        cfg.maxSpeed = 420.0f;
        cfg.minSize = 3.0f;
        cfg.maxSize = 6.0f;
        cfg.spread = 2.0f * PI;
        cfg.direction = 0.0f;
        cfg.gravity = {0.0f, 120.0f};
        cfg.startColor = {255, 220, 120, 255};
        cfg.endColor = {255, 80, 40, 0};
        applyConfigToEmitter(cfg);
        mouseConfig_ = cfg;
        streaksEnabled_ = false;
    }

    void applyPresetSparks() {
        ContinuousEmitterConfig cfg = mouseConfig_;
        cfg.emitRate = 1200.0f;
        cfg.minLifetime = 0.15f;
        cfg.maxLifetime = 0.4f;
        cfg.minSpeed = 250.0f;
        cfg.maxSpeed = 600.0f;
        cfg.minSize = 2.0f;
        cfg.maxSize = 4.0f;
        cfg.spread = PI * 0.35f;
        cfg.direction = -PI * 0.5f;
        cfg.gravity = {0.0f, 600.0f};
        cfg.startColor = YELLOW;
        cfg.endColor = Fade(ORANGE, 0.1f);
        applyConfigToEmitter(cfg);
        mouseConfig_ = cfg;
        streaksEnabled_ = true;
    }
    ParticleEmitterEntity* emitterEntity_{nullptr};
    ParticleEmitterEntity* rainEmitterEntity_{nullptr};
    ContinuousParticleEmitter* emitter_{nullptr};
    ContinuousParticleEmitter* rainEmitter_{nullptr};
    FpsCounter* fps_{nullptr};
    rlge::Camera camera_;
    int burstCount_{150};
    bool streaksEnabled_{false};
    ContinuousEmitterConfig mouseConfig_{};
};

int main() {
    constexpr WindowConfig cfg{
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
