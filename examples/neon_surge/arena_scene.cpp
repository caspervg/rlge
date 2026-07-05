#include "arena_scene.hpp"

#include <algorithm>
#include <cmath>

#include "imgui.h"
#include "raymath.h"
#include "runtime.hpp"

#include "backdrop.hpp"
#include "bullet.hpp"
#include "enemy.hpp"
#include "fx.hpp"
#include "hud.hpp"
#include "ns_config.hpp"
#include "ns_game.hpp"
#include "pickup.hpp"
#include "player.hpp"

namespace neon {
    using namespace rlge;

    namespace {
        Color colorFor(const EnemyKind kind) {
            switch (kind) {
            case EnemyKind::Chaser: return pal::chaser;
            case EnemyKind::Weaver: return pal::weaver;
            case EnemyKind::Splitter: return pal::splitter;
            case EnemyKind::Shard: return pal::shard;
            case EnemyKind::Comet: return pal::comet;
            }
            return WHITE;
        }

        float frand01() { return static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f; }

        constexpr float kPortalDuration = 0.8f;
    } // namespace

    // ------------------------------------------------------------ SpawnPortal

    SpawnPortal::SpawnPortal(Scene& scene, NsGame* game, const EnemyKind kind, const Vector2 pos,
                             const int wave) :
        RenderEntity(scene), game_(game), kind_(kind), pos_(pos), wave_(wave) {}

    void SpawnPortal::update(const float dt) {
        RenderEntity::update(dt);
        if (done_)
            return;

        t_ += dt;
        if (t_ >= kPortalDuration) {
            done_ = true;
            NsGame* game = game_;
            const EnemyKind kind = kind_;
            const Vector2 pos = pos_;
            const int wave = wave_;
            static_cast<ArenaScene&>(scene()).deferSpawn([game, kind, pos, wave](Scene& s) {
                s.spawn<Enemy>(game, kind, pos, wave);
            });
            destroyDeferred();
        }
    }

    void SpawnPortal::draw() {
        RenderEntity::draw();
        if (done_)
            return;

        const float progress = std::clamp(t_ / kPortalDuration, 0.0f, 1.0f);
        const Vector2 pos = pos_;
        const Color color = colorFor(kind_);

        const Texture2D& glowTex = game_->assets().glow;
        const float glowSize = 40.0f + 60.0f * progress;
        rq().submitSprite(game_->glowLayer(), 0.5f, glowTex,
                          Rectangle{0.0f, 0.0f, static_cast<float>(glowTex.width),
                                    static_cast<float>(glowTex.height)},
                          Rectangle{pos.x, pos.y, glowSize, glowSize},
                          Vector2{glowSize * 0.5f, glowSize * 0.5f}, 0.0f,
                          Fade(color, 0.25f + 0.35f * progress));

        rq().submitWorld(1.0f, [pos, color, progress] {
            const float radius = 54.0f - 40.0f * progress;
            const float alpha = 0.25f + 0.75f * progress;
            DrawRing(pos, radius - 2.5f, radius, 0.0f, 360.0f, 36, Fade(color, alpha));
            // Spinning tick marks converging with the ring.
            const float spin = progress * 6.0f;
            for (int i = 0; i < 3; ++i) {
                const float a = spin + static_cast<float>(i) * (2.0f * PI / 3.0f);
                const Vector2 from{pos.x + std::cos(a) * (radius + 8.0f), pos.y + std::sin(a) * (radius + 8.0f)};
                const Vector2 to{pos.x + std::cos(a) * (radius + 2.0f), pos.y + std::sin(a) * (radius + 2.0f)};
                DrawLineEx(from, to, 2.0f, Fade(color, alpha));
            }
        });
    }

    // ------------------------------------------------------------- ArenaScene

    ArenaScene::ArenaScene(Runtime& r, NsGame* game) :
        Scene(r), game_(game) {}

    void ArenaScene::enter() {
        collisions().setSpatialGridSize(140.0f);

        lastPlayerPos_ = {cfg.arenaWidth * 0.5f, cfg.arenaHeight * 0.5f};
        camera_.setTarget(lastPlayerPos_);
        setSingleView(camera_);

        // Backdrop with generous margins so screen edges never show past it.
        backdrop_ = &spawn<NebulaBackdrop>(game_, Rectangle{-900.0f, -900.0f, cfg.arenaWidth + 1800.0f,
                                                            cfg.arenaHeight + 1800.0f});
        spawn<Starfield>(100);
        spawn<ArenaFrame>();

        player_ = &spawn<Player>(game_, lastPlayerPos_);
        hud_ = &spawn<Hud>(game_);

        collisionResponses().addHandler([this](Entity& entity, const CollisionEvent& event) {
            if (event.state == CollisionState::Exit)
                return;
            Entity& other = event.colliderB->entity();

            if (auto* bullet = dynamic_cast<Bullet*>(&entity)) {
                if (auto* enemy = dynamic_cast<Enemy*>(&other)) {
                    if (enemy->alive()) {
                        bullet->hitEnemy(*enemy);
                    }
                }
            } else if (auto* player = dynamic_cast<Player*>(&entity)) {
                if (auto* enemy = dynamic_cast<Enemy*>(&other)) {
                    if (enemy->alive() && player->alive()) {
                        Vector2 dir = Vector2Subtract(player->pos(), enemy->pos());
                        if (Vector2LengthSqr(dir) > 0.01f) {
                            dir = Vector2Normalize(dir);
                        } else {
                            dir = {0.0f, -1.0f};
                        }
                        const bool landed = godMode_ || player->takeHit(dir);
                        if (landed) {
                            enemy->contactPlayer();
                        }
                    }
                } else if (auto* pickup = dynamic_cast<Pickup*>(&other)) {
                    if (player->alive()) {
                        pickup->collect(*player);
                    }
                }
            }
        });

        subscribeEvents_();

        // Ambient synth pad heartbeat.
        game_->assets().sfx.play("pad", 0.6f);
        timers().every(4.1f, [this] { game_->assets().sfx.play("pad", 0.6f); });
    }

    void ArenaScene::subscribeEvents_() {
        sceneEvents().subscribe<EnemyKilled>([this](const EnemyKilled& e) { onEnemyKilled_(e); });
        sceneEvents().subscribe<PlayerDamaged>([this](const PlayerDamaged& e) { onPlayerDamaged_(e); });
        sceneEvents().subscribe<PlayerDied>([this](const PlayerDied& e) { onPlayerDied_(e); });
        sceneEvents().subscribe<PowerUpCollected>([this](const PowerUpCollected& e) { onPowerUpCollected_(e); });
    }

    void ArenaScene::update(const float dt) {
        // Pause / quit handling works even while frozen.
        if (!runOver_ && input().keyPressed(KeyCode::P)) {
            paused_ = !paused_;
        }
        if (input().keyPressed(KeyCode::Escape)) {
            if (paused_) {
                gameEvents().enqueue(BackToMenuRequested{});
            } else if (!runOver_) {
                paused_ = true;
            }
        }
        if (paused_)
            return;

        Scene::update(dt);

        // Flush deferred spawns (index loop: spawns may queue more spawns).
        for (std::size_t i = 0; i < spawnQueue_.size(); ++i) {
            spawnQueue_[i](*this);
        }
        spawnQueue_.clear();

        if (player_) {
            lastPlayerPos_ = player_->pos();
        }

        // Combo decay.
        if (comboTimer_ > 0.0f) {
            comboTimer_ -= dt;
            if (comboTimer_ <= 0.0f) {
                combo_ = 0;
                mult_ = 1;
            }
        }

        if (!runOver_) {
            updateDirector_(dt);
        }

        // Nebula reflects how much trouble we are in.
        if (backdrop_) {
            const float danger = player_
                ? static_cast<float>(cfg.playerMaxHp - player_->hp()) /
                      static_cast<float>(cfg.playerMaxHp) * 0.75f
                : 1.0f;
            backdrop_->setDanger(danger);
        }

        updateCamera_(dt);
    }

    void ArenaScene::updateCamera_(const float dt) {
        const View* view = primaryView();
        if (!view)
            return;
        const Rectangle vp = view->viewport;

        zoomKick_ = std::max(0.0f, zoomKick_ - dt * 0.25f);

        const float baseZoom = vp.height > 0.0f ? vp.height / cfg.screenHeight * cfg.camZoom : 1.0f;
        const float zoom = baseZoom * (1.0f + zoomKick_);
        camera_.setOffset({vp.x + vp.width * 0.5f, vp.y + vp.height * 0.5f});
        camera_.setZoom(zoom);
        camera_.follow(lastPlayerPos_, cfg.camLerp);

        // Keep the view inside the arena (with a bit of border allowance).
        const float halfW = vp.width * 0.5f / zoom;
        const float halfH = vp.height * 0.5f / zoom;
        constexpr float pad = 90.0f;
        Vector2 target = camera_.target();
        if (halfW * 2.0f < cfg.arenaWidth + pad * 2.0f) {
            target.x = std::clamp(target.x, halfW - pad, cfg.arenaWidth - halfW + pad);
        }
        if (halfH * 2.0f < cfg.arenaHeight + pad * 2.0f) {
            target.y = std::clamp(target.y, halfH - pad, cfg.arenaHeight - halfH + pad);
        }
        camera_.setTarget(target);
    }

    // --------------------------------------------------------------- Director

    void ArenaScene::updateDirector_(const float dt) {
        switch (phase_) {
        case Phase::Intermission:
            phaseTimer_ -= dt;
            if (phaseTimer_ <= 0.0f) {
                wave_ += 1;
                budget_ = std::min(6 + wave_ * 4, 70);
                groupTimer_ = 0.4f;
                phase_ = Phase::Spawning;
                sceneEvents().publish(WaveStarted{wave_});
                game_->assets().sfx.play("wavehorn", 0.7f);
            }
            break;

        case Phase::Spawning:
            groupTimer_ -= dt;
            if (groupTimer_ <= 0.0f) {
                spawnGroup_();
                groupTimer_ = std::clamp(1.7f - static_cast<float>(wave_) * 0.06f, 0.7f, 1.7f);
            }
            if (budget_ <= 0) {
                phase_ = Phase::Fighting;
            }
            break;

        case Phase::Fighting:
            if (enemiesAlive() == 0) {
                const long long bonus = static_cast<long long>(wave_) * 150;
                score_ += bonus;
                sceneEvents().publish(WaveCleared{wave_});
                sceneEvents().publish(ScoreChanged{score_, mult_});
                if (player_) {
                    fx::floatingText(*this, Vector2Add(playerPos(), {0.0f, -40.0f}),
                                     TextFormat("WAVE BONUS +%lld", bonus), pal::pickupHeal, 20.0f);
                }
                game_->assets().sfx.play("wavehorn", 0.5f, 1.4f);
                phase_ = Phase::Intermission;
                phaseTimer_ = 2.6f;
            }
            break;
        }
    }

    void ArenaScene::spawnGroup_() {
        const int groupSize = std::min({budget_, 2 + wave_, 6});
        for (int i = 0; i < groupSize && budget_ > 0; ++i) {
            EnemyKind kind = pickEnemyKind_();
            int cost = 1;
            switch (kind) {
            case EnemyKind::Splitter: cost = 3; break;
            case EnemyKind::Comet:
            case EnemyKind::Weaver: cost = 2; break;
            default: cost = 1; break;
            }
            if (cost > budget_) {
                kind = EnemyKind::Chaser;
                cost = 1;
            }
            budget_ -= cost;
            spawnPortalAt_(kind, pickSpawnPos_());
        }
    }

    EnemyKind ArenaScene::pickEnemyKind_() const {
        const int chaserW = 50;
        const int weaverW = wave_ >= 2 ? 22 + wave_ * 2 : 0;
        const int cometW = wave_ >= 3 ? 14 + wave_ * 2 : 0;
        const int splitterW = wave_ >= 4 ? 10 + wave_ * 2 : 0;
        const int total = chaserW + weaverW + cometW + splitterW;

        int roll = GetRandomValue(0, total - 1);
        if ((roll -= chaserW) < 0) return EnemyKind::Chaser;
        if ((roll -= weaverW) < 0) return EnemyKind::Weaver;
        if ((roll -= cometW) < 0) return EnemyKind::Comet;
        return EnemyKind::Splitter;
    }

    Vector2 ArenaScene::pickSpawnPos_() const {
        const float angle = frand01() * 2.0f * PI;
        const float dist = 520.0f + frand01() * 330.0f;
        Vector2 pos = Vector2Add(lastPlayerPos_, {std::cos(angle) * dist, std::sin(angle) * dist});
        constexpr float margin = 70.0f;
        pos.x = std::clamp(pos.x, margin, cfg.arenaWidth - margin);
        pos.y = std::clamp(pos.y, margin, cfg.arenaHeight - margin);
        return pos;
    }

    void ArenaScene::spawnPortalAt_(const EnemyKind kind, const Vector2 pos) {
        spawn<SpawnPortal>(game_, kind, pos, wave_);
    }

    // ----------------------------------------------------------------- Events

    void ArenaScene::onEnemyKilled_(const EnemyKilled& e) {
        kills_ += 1;
        combo_ += 1;
        comboTimer_ = cfg.comboWindow;
        mult_ = std::min(1 + combo_ / cfg.comboPerMult, cfg.comboMaxMult);

        const long long gained = static_cast<long long>(e.baseScore) * mult_;
        score_ += gained;
        sceneEvents().publish(ScoreChanged{score_, mult_});

        const Color textColor = mult_ >= 6 ? pal::chaser : (mult_ >= 3 ? pal::comet : pal::hudText);
        fx::floatingText(*this, e.pos, TextFormat("+%lld", gained), textColor,
                         14.0f + static_cast<float>(mult_) * 1.5f);

        const bool big = e.kind == EnemyKind::Splitter;
        camera_.shake(big ? 5.0f : 2.5f, big ? 0.3f : 0.16f);
        addZoomKick(big ? 0.025f : 0.008f);

        maybeDropPickup_(e.pos);
    }

    void ArenaScene::maybeDropPickup_(const Vector2 pos) {
        if (frand01() < cfg.gemChance) {
            spawn<Pickup>(game_, PickupType::ScoreGem, pos);
        }
        if (frand01() < cfg.dropChance) {
            PickupType type;
            const int roll = GetRandomValue(0, 99);
            if (roll < 30) type = PickupType::RapidFire;
            else if (roll < 60) type = PickupType::TripleShot;
            else if (roll < 82) type = PickupType::Shield;
            else type = PickupType::Heal;

            if (type == PickupType::Heal && player_ && player_->hp() >= cfg.playerMaxHp) {
                type = PickupType::Shield;
            }
            spawn<Pickup>(game_, type, Vector2Add(pos, {18.0f, -12.0f}));
        }
    }

    void ArenaScene::onPlayerDamaged_(const PlayerDamaged&) {
        combo_ = 0;
        mult_ = 1;
        comboTimer_ = 0.0f;
        camera_.shake(7.0f, 0.4f);
        addZoomKick(0.035f);
    }

    void ArenaScene::onPlayerDied_(const PlayerDied&) {
        runOver_ = true;
        player_ = nullptr;
        camera_.shake(11.0f, 0.7f);
        addZoomKick(0.06f);
        game_->assets().sfx.play("gameover", 0.9f);

        const GameOverStats stats{score_, wave_, kills_, false};
        timers().after(1.8f, [this, stats] {
            gameEvents().enqueue(stats);
        });
    }

    void ArenaScene::onPowerUpCollected_(const PowerUpCollected& e) {
        game_->assets().sfx.play("pickup", 0.6f, 1.0f, 0.06f);
        if (e.type == PickupType::Shield) {
            game_->assets().sfx.play("shield", 0.5f, 1.2f);
        }
        if (e.type == PickupType::ScoreGem) {
            const long long gained = 500LL * mult_;
            score_ += gained;
            sceneEvents().publish(ScoreChanged{score_, mult_});
            fx::floatingText(*this, e.pos, TextFormat("+%lld", gained), pal::pickupGem, 16.0f);
        }
    }

    // ---------------------------------------------------------------- Helpers

    void ArenaScene::deferSpawn(std::function<void(Scene&)> fn) {
        spawnQueue_.push_back(std::move(fn));
    }

    void ArenaScene::addZoomKick(const float amount) {
        zoomKick_ = std::min(zoomKick_ + amount, 0.12f);
    }

    bool ArenaScene::playerAlive() const { return player_ != nullptr && player_->alive(); }

    float ArenaScene::comboFrac() const {
        return cfg.comboWindow > 0.0f ? std::clamp(comboTimer_ / cfg.comboWindow, 0.0f, 1.0f) : 0.0f;
    }

    int ArenaScene::enemiesAlive() {
        int count = 0;
        for (const auto& ent : entities()) {
            if (!ent)
                continue;
            if (const auto* enemy = dynamic_cast<const Enemy*>(ent.get())) {
                if (enemy->alive())
                    count += 1;
            } else if (dynamic_cast<const SpawnPortal*>(ent.get()) != nullptr) {
                count += 1;
            }
        }
        return count;
    }

    void ArenaScene::debugOverlay() {
        if (ImGui::Begin("Neon Surge")) {
            ImGui::Text("wave %d  phase %d  budget %d", wave_, static_cast<int>(phase_), budget_);
            ImGui::Text("score %lld  x%d  combo %d", score_, mult_, combo_);
            ImGui::Text("enemies %d  kills %d", enemiesAlive(), kills_);
            ImGui::Checkbox("God mode", &godMode_);
            if (ImGui::Button("Kill all enemies")) {
                // Snapshot first: dying splitters spawn shards, which mutates the entity list.
                std::vector<Enemy*> targets;
                for (const auto& ent : entities()) {
                    if (auto* enemy = dynamic_cast<Enemy*>(ent.get())) {
                        if (enemy->alive()) {
                            targets.push_back(enemy);
                        }
                    }
                }
                for (auto* enemy : targets) {
                    enemy->takeDamage(999, {0.0f, -1.0f});
                }
            }
            if (ImGui::Button("Skip wave") && phase_ != Phase::Intermission) {
                budget_ = 0;
                phase_ = Phase::Fighting;
            }

            const auto& stats = rq().stats();
            ImGui::Separator();
            ImGui::Text("sprites %zu  batches %zu  cmds %zu", stats.spritesSubmitted, stats.batchCount,
                        stats.customCommands);
        }
        ImGui::End();
    }

} // namespace neon
