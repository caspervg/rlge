#include <cmath>

#include "action.hpp"
#include "input.hpp"
#include "render_layer.hpp"
#include "render_queue.hpp"
#include "sea_creature.hpp"
#include "sprite.hpp"
#include "transformer.hpp"

namespace anim_demo {
    SeaCreature::SeaCreature(rlge::Scene& scene, rlge::SpriteAtlas<AnimState>& atlas, Shader flashShader,
        rlge::ShaderHandle flashHandle, rlge::Camera2DController& camera, const rlge::LayerId bubbleLayer)
        : RenderEntity(scene)
        , atlas_(atlas)
        , camera_(camera) {
        auto& tr = add<rlge::Transform>();
        tr.position = {0.0f, 0.0f};
        tr.scale = {kScale, kScale};

        auto& sprite = add<rlge::SpriteAnim>(atlas_.texture(), atlas_.frameW(), atlas_.frameH());
        atlas_.addTo(sprite);
        sprite.setAutoAdvance(false);

        flash_ = &add<rlge::ShaderEffect<FlashParams>>(flashShader, flashHandle)
                      .bind("u_intensity", &FlashParams::intensity)
                      .bind("u_flashColor", &FlashParams::flashColor);

        rlge::ContinuousEmitterConfig bubbleConfig;
        bubbleConfig.localOffset = {0.0f, -kFrameH * 0.05f};
        bubbleConfig.emitRate = 18.0f;
        bubbleConfig.maxParticles = 140;
        bubbleConfig.minLifetime = 0.5f;
        bubbleConfig.maxLifetime = 1.2f;
        bubbleConfig.minSpeed = 20.0f;
        bubbleConfig.maxSpeed = 60.0f;
        bubbleConfig.minSize = 2.0f;
        bubbleConfig.maxSize = 5.5f;
        bubbleConfig.spread = 0.7f;
        bubbleConfig.direction = -1.57079633f;
        bubbleConfig.gravity = {0.0f, -8.0f};
        bubbleConfig.startColor = Color{110, 190, 235, 160};
        bubbleConfig.endColor = Color{170, 230, 255, 0};

        bubbleEmitter_ = &add<rlge::ContinuousParticleEmitter>(bubbleConfig, [](const rlge::Particle& p) {
            DrawCircleV(p.pos, p.size, p.color);
            DrawCircleLines(static_cast<int>(p.pos.x), static_cast<int>(p.pos.y), p.size, Color{210, 245, 255, 80});
        });
        bubbleEmitter_->setSpawnFn([](const Vector2 origin) {
            return rlge::spawnInCircle(origin, 16.0f);
        });
        bubbleEmitter_->setLayer(bubbleLayer);
        bubbleEmitter_->stop();

        anim_ = &add<rlge::AnimationStateMachine<AnimState>>(sprite);
        registerClips_();
        anim_->setState(AnimState::Idle);
    }

    void SeaCreature::update(const float dt) {
        handleInput_(dt);
        RenderEntity::update(dt);
        updateFlash_(dt);

        if (eventTimer_ > 0.0f) {
            eventTimer_ -= dt;
            if (eventTimer_ < 0.0f) {
                eventTimer_ = 0.0f;
            }
        }

        if (queueIdle_ && anim_ && anim_->currentState() != AnimState::Death) {
            anim_->setState(AnimState::Idle);
            queueIdle_ = false;
        }
    }

    void SeaCreature::draw() {
        if (const auto* tr = get<rlge::Transform>()) {
            const float scaleX = std::abs(tr->scale.x);
            const float scaleY = std::abs(tr->scale.y);
            const float shadowW = kFrameW * scaleX * 0.45f;
            const float shadowH = kFrameH * scaleY * 0.22f;
            const Vector2 shadowPos{tr->position.x, tr->position.y + kFrameH * scaleY * 0.35f};
            const rlge::LayerId layer = scene().layers().world();
            rq().submit(layer, shadowPos.y - 2.0f, [shadowPos, shadowW, shadowH] {
                DrawEllipse(static_cast<int>(shadowPos.x), static_cast<int>(shadowPos.y),
                            shadowW, shadowH, Color{10, 20, 30, 90});
            });
        }

        RenderEntity::draw();
    }

    AnimState SeaCreature::state() const {
        if (!anim_) {
            return AnimState::Idle;
        }
        return anim_->currentState();
    }

    const std::string& SeaCreature::lastEvent() const { return lastEvent_; }

    float SeaCreature::eventTimer() const { return eventTimer_; }

    void SeaCreature::registerClips_() {
        if (!anim_) {
            return;
        }
        for (const auto& clip : atlas_.clips()) {
            rlge::AnimationClip data{
                .startFrame = clip.startFrame,
                .frameCount = clip.frameCount,
                .frameTime = clip.frameTime,
                .loop = clip.loop
            };
            if (clip.state == AnimState::Attack) {
                data.onComplete = [this] {
                    queueIdle_ = true;
                    pushEvent_("Attack complete");
                };
            } else if (clip.state == AnimState::Hurt) {
                data.onComplete = [this] {
                    queueIdle_ = true;
                    pushEvent_("Hurt complete");
                };
            } else if (clip.state == AnimState::Death) {
                data.onComplete = [this] {
                    pushEvent_("Death complete");
                };
            }
            anim_->registerClip(clip.state, data);
        }
    }

    void SeaCreature::handleInput_(float dt) {
        auto* tr = get<rlge::Transform>();
        if (!tr || !anim_) {
            return;
        }
        moving_ = false;

        const auto& input = scene().input();
        const bool attackPressed = input.pressed(Action::Attack);
        const bool movingLeft = input.down(Action::MoveLeft);
        const bool movingRight = input.down(Action::MoveRight);
        const bool hurtPressed = input.pressed(Action::Hurt);
        const bool deathPressed = input.pressed(Action::Death);
        const bool resetPressed = input.pressed(Action::Reset);

        if (anim_->currentState() == AnimState::Death) {
            if (resetPressed) {
                anim_->setState(AnimState::Idle);
                queueIdle_ = false;
                triggerFlash_({1.0f, 1.0f, 1.0f}, 0.12f, 0.4f);
                pushEvent_("Reset");
            }
            return;
        }

        if (anim_->currentState() == AnimState::Attack || anim_->currentState() == AnimState::Hurt) {
            return;
        }

        if (deathPressed) {
            anim_->setState(AnimState::Death);
            queueIdle_ = false;
            triggerFlash_({0.4f, 0.4f, 0.4f}, 0.35f, 0.7f);
            pushEvent_("Death start");
            return;
        }

        if (hurtPressed) {
            anim_->setState(AnimState::Hurt);
            queueIdle_ = false;
            triggerFlash_({1.0f, 0.25f, 0.25f}, 0.22f, 0.9f);
            camera_.shake(0.5f, 0.15f);
            pushEvent_("Hurt start");
            return;
        }

        if (attackPressed) {
            anim_->setState(AnimState::Attack);
            queueIdle_ = false;
            triggerFlash_({1.0f, 0.85f, 0.35f}, 0.18f, 0.85f);
            pushEvent_("Attack start");
            return;
        }

        if (movingLeft || movingRight) {
            anim_->setState(AnimState::Walk);
            constexpr auto speed = 60.0f;
            if (movingLeft) {
                facingLeft_ = true;
            } else if (movingRight) {
                facingLeft_ = false;
            }
            tr->position.x += (movingRight ? 1.0f : -1.0f) * speed * dt;
            moving_ = true;
        } else {
            anim_->setState(AnimState::Idle);
        }

        tr->scale.x = facingLeft_ ? -kScale : kScale;
        if (bubbleEmitter_) {
            if (moving_ && !bubbleEmitter_->emitting()) {
                bubbleEmitter_->start();
            } else if (!moving_ && bubbleEmitter_->emitting()) {
                bubbleEmitter_->stop();
            }
        }
    }

    void SeaCreature::triggerFlash_(const Vector3 color, const float duration, const float intensity) {
        if (!flash_) {
            return;
        }
        flashDuration_ = duration;
        flashTimer_ = duration;
        flashMaxIntensity_ = intensity;
        flash_->params().flashColor = color;
    }

    void SeaCreature::updateFlash_(const float dt) {
        if (!flash_) {
            return;
        }
        if (flashTimer_ > 0.0f) {
            flashTimer_ -= dt;
            if (flashTimer_ < 0.0f) {
                flashTimer_ = 0.0f;
            }
            const float t = (flashDuration_ > 0.0f) ? (flashTimer_ / flashDuration_) : 0.0f;
            flash_->params().intensity = t * flashMaxIntensity_;
        } else {
            flash_->params().intensity = 0.0f;
        }
    }

    void SeaCreature::pushEvent_(const char* label) {
        lastEvent_ = label;
        eventTimer_ = eventLabelDuration_;
    }
}
