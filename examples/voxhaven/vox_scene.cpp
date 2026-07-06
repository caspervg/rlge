#include "vox_scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "imgui.h"
#include "raymath.h"
#include "render_queue.hpp"
#include "rlgl.h"
#include "runtime.hpp"

#include "vx_mesher.hpp"

namespace vox {
    using namespace rlge;

    namespace {
        constexpr auto kLandVertexShader = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

        constexpr auto kWaterVertexShader = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform float u_time;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
void main() {
    vec3 pos = vertexPosition;
    pos.y += sin(u_time * 1.6 + vertexPosition.x * 0.9 + vertexPosition.z * 0.7) * 0.05;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = vec3(matModel * vec4(pos, 1.0));
    gl_Position = mvp * vec4(pos, 1.0);
}
)";

        constexpr auto kChunkFragmentShader = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
uniform sampler2D texture0;
uniform vec3 u_tint;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform vec3 u_camPos;
out vec4 finalColor;
void main() {
    vec4 tex = texture(texture0, fragTexCoord);
    if (tex.a < 0.35) discard;
    vec3 col = tex.rgb * fragColor.rgb * u_tint;
    float dist = length(fragPosition - u_camPos);
    float fog = smoothstep(u_fogRange.x, u_fogRange.y, dist);
    col = mix(col, u_fogColor, fog);
    finalColor = vec4(col, tex.a * fragColor.a);
}
)";

        Vector3 toV3(const Color c) {
            return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f};
        }

        Color fromV3(const Vector3 v, const unsigned char a = 255) {
            const auto ch = [](const float f) {
                return static_cast<unsigned char>(std::clamp(f * 255.0f, 0.0f, 255.0f));
            };
            return {ch(v.x), ch(v.y), ch(v.z), a};
        }

        Texture2D makeDiscTexture(const Color inner, const Color rim, const bool craters) {
            constexpr int size = 32;
            Image img = GenImageColor(size, size, BLANK);
            constexpr float center = (size - 1) * 0.5f;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const float dx = (x - center) / center;
                    const float dy = (y - center) / center;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    if (d > 1.0f)
                        continue;
                    Color c = d < 0.72f ? inner : rim;
                    if (craters && ((x * 7 + y * 13) % 23 == 0) && d < 0.7f) {
                        c = rim;
                    }
                    c.a = d > 0.9f ? static_cast<unsigned char>(255 * (1.0f - d) * 10.0f) : 255;
                    ImageDrawPixel(&img, x, y, c);
                }
            }
            Texture2D tex = LoadTextureFromImage(img);
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            UnloadImage(img);
            return tex;
        }

        void drawLabel(const Vector2 at, const char* text, const float size, const Color color) {
            DrawTextEx(GetFontDefault(), text, at, size, size / 10.0f, color);
        }

        Vector2 measure(const char* text, const float size) {
            return MeasureTextEx(GetFontDefault(), text, size, size / 10.0f);
        }

        float frand01() { return static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f; }
    } // namespace

    VoxScene::VoxScene(Runtime& r) :
        Scene(r) {}

    VoxScene::~VoxScene() {
        world_.save();
        if (materialsReady_) {
            // Shaders belong to the AssetStore; detach before the material unload.
            matLand_.shader = Shader{};
            matWater_.shader = Shader{};
        }
        if (atlas_.id != 0) UnloadTexture(atlas_);
        if (sunTex_.id != 0) UnloadTexture(sunTex_);
        if (moonTex_.id != 0) UnloadTexture(moonTex_);
    }

    void VoxScene::enter() {
        SetExitKey(KEY_NULL); // ESC pauses; quitting goes through the pause menu
        atlas_ = buildAtlas(world_.seed());
        sunTex_ = makeDiscTexture(Color{255, 236, 160, 255}, Color{255, 190, 90, 255}, false);
        moonTex_ = makeDiscTexture(Color{224, 228, 240, 255}, Color{160, 168, 190, 255}, true);

        setupShaders_();
        setupView_();

        // Fixed star dome + drifting cloud field.
        starDirs_.reserve(220);
        for (int i = 0; i < 220; ++i) {
            const float az = frand01() * 2.0f * PI;
            const float el = 0.05f + frand01() * 1.35f;
            starDirs_.push_back({std::cos(az) * std::cos(el), std::sin(el), std::sin(az) * std::cos(el)});
        }
        clouds_.reserve(36);
        for (int i = 0; i < 36; ++i) {
            clouds_.push_back({{frand01() * 480.0f, frand01() * 480.0f},
                               10.0f + frand01() * 26.0f, 8.0f + frand01() * 18.0f,
                               0.22f + frand01() * 0.16f});
        }

        warmStart_();
        player_.spawn(world_, spawnPos_.x, spawnPos_.z);
        spawnPos_ = player_.position();

        // Feedback effects ride the scene event bus, not the edit code path.
        sceneEvents().subscribe<BlockBroken>([this](const BlockBroken& e) {
            const Color c = blockInfo(e.block).mapColor;
            for (int i = 0; i < 14; ++i) {
                debris_.push_back({{e.x + 0.5f, e.y + 0.5f, e.z + 0.5f},
                                   {(frand01() - 0.5f) * 5.0f, 2.0f + frand01() * 4.0f,
                                    (frand01() - 0.5f) * 5.0f},
                                   c, 0.5f + frand01() * 0.4f, 0.07f + frand01() * 0.08f});
            }
            sfx_.play("break", 0.7f, 1.0f, 0.15f);
        });
        sceneEvents().subscribe<BlockPlaced>([this](const BlockPlaced&) {
            sfx_.play("place", 0.6f, 1.0f, 0.12f);
        });

        // Engine timers: autosave + ambient wind gusts.
        timers().every(cfg.autosaveSeconds, [this] { world_.save(); });
        timers().every(9.0f, [this] {
            if (frand01() < 0.6f)
                sfx_.play("wind", 0.3f, 1.0f, 0.2f);
        });
        sfx_.play("wind", 0.35f);
    }

    void VoxScene::exit() {
        world_.save();
        EnableCursor();
    }

    void VoxScene::setupShaders_() {
        auto& store = runtime().assetStore();
        const ShaderHandle landHandle =
            store.loadShaderFromMemory("vox_land", kLandVertexShader, kChunkFragmentShader);
        const ShaderHandle waterHandle =
            store.loadShaderFromMemory("vox_water", kWaterVertexShader, kChunkFragmentShader);
        Shader& land = store.shader(landHandle);
        Shader& water = store.shader(waterHandle);

        locLandTint_ = GetShaderLocation(land, "u_tint");
        locLandFogColor_ = GetShaderLocation(land, "u_fogColor");
        locLandFogRange_ = GetShaderLocation(land, "u_fogRange");
        locLandCamPos_ = GetShaderLocation(land, "u_camPos");
        locWaterTint_ = GetShaderLocation(water, "u_tint");
        locWaterFogColor_ = GetShaderLocation(water, "u_fogColor");
        locWaterFogRange_ = GetShaderLocation(water, "u_fogRange");
        locWaterCamPos_ = GetShaderLocation(water, "u_camPos");
        locWaterTime_ = GetShaderLocation(water, "u_time");

        matLand_ = LoadMaterialDefault();
        matLand_.shader = land;
        matLand_.maps[MATERIAL_MAP_ALBEDO].texture = atlas_;
        matWater_ = LoadMaterialDefault();
        matWater_.shader = water;
        matWater_.maps[MATERIAL_MAP_ALBEDO].texture = atlas_;
        materialsReady_ = true;
    }

    void VoxScene::setupView_() {
        cam3_.setFovy(72.0f);
        cam3_.setPosition({0.0f, 50.0f, 0.0f});
        cam3_.setTarget({10.0f, 45.0f, 10.0f});
        const auto [w, h] = runtime().window().size();
        addView3D(cam3_, Rectangle{0.0f, 0.0f, w, h},
                  [](const float width, const float height) {
                      return Rectangle{0.0f, 0.0f, width, height};
                  });
    }

    void VoxScene::warmStart_() {
        // Generate + mesh the spawn area synchronously so the menu shows a world.
        for (int i = 0; i < 60; ++i) {
            world_.update(spawnPos_);
        }
        for (int i = 0; i < 80; ++i) {
            if (Mesher::remeshDirty(world_, spawnPos_, 4) == 0)
                break;
        }
    }

    void VoxScene::setState_(const State s) {
        state_ = s;
        if (s == State::Playing) {
            DisableCursor();
            skipLookFrames_ = 2;
        } else {
            EnableCursor();
        }
    }

    // ------------------------------------------------------------- Update

    void VoxScene::update(const float dt) {
        Scene::update(dt);
        worldClock_ += dt;

        switch (state_) {
        case State::Menu: {
            menuOrbit_ += dt * 0.12f;
            const Vector3 center{spawnPos_.x, spawnPos_.y + 4.0f, spawnPos_.z};
            cam3_.setPosition({center.x + std::cos(menuOrbit_) * 30.0f,
                               center.y + 13.0f,
                               center.z + std::sin(menuOrbit_) * 30.0f});
            cam3_.setTarget(center);
            world_.update(spawnPos_);
            meshedThisFrame_ = Mesher::remeshDirty(world_, spawnPos_, cfg.meshPerFrame);
            updateDayCycle_(dt);
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE)) {
                setState_(State::Playing);
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                runtime().quit();
            }
            break;
        }
        case State::Playing:
            updatePlaying_(dt);
            break;
        case State::Paused:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
                setState_(State::Playing);
            }
            if (IsKeyPressed(KEY_Q)) {
                world_.save();
                runtime().quit();
            }
            break;
        }

        updateDebris_(dt);
    }

    void VoxScene::updatePlaying_(const float dt) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
            setState_(State::Paused);
            return;
        }
        if (IsKeyPressed(KEY_F)) {
            player_.toggleFly();
        }

        // Gather inputs (engine axis bindings + raw keys).
        PlayerController::Inputs in;
        in.moveX = input().axisValue(Action::MoveRight);
        in.moveZ = -input().axisValue(Action::MoveDown); // W = forward
        in.jump = IsKeyDown(KEY_SPACE);
        in.sprint = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SHIFT);
        in.descend = IsKeyDown(KEY_LEFT_SHIFT) && player_.flying();
        if (skipLookFrames_ > 0) {
            skipLookFrames_--;
        } else {
            in.lookDelta = GetMouseDelta();
        }

        player_.update(world_, in, dt);
        if (player_.justJumped()) sfx_.play("jump", 0.25f, 1.0f, 0.1f);
        if (player_.justSplashed()) sfx_.play("splash", 0.6f, 1.0f, 0.1f);

        // Camera follows the eye.
        const Vector3 eye = player_.eyePosition();
        const Vector3 look = player_.lookDir();
        cam3_.setPosition(eye);
        cam3_.setTarget(Vector3Add(eye, look));

        // World streaming + meshing budget.
        world_.update(player_.position());
        meshedThisFrame_ = Mesher::remeshDirty(world_, player_.position(), cfg.meshPerFrame);

        updateDayCycle_(IsKeyDown(KEY_T) ? dt * 40.0f : dt);
        updateInteraction_(dt);

        // Hotbar selection.
        const float wheel = GetMouseWheelMove();
        if (wheel < -0.1f) hotbarIndex_ = (hotbarIndex_ + 1) % 9;
        if (wheel > 0.1f) hotbarIndex_ = (hotbarIndex_ + 8) % 9;
        for (int i = 0; i < 9; ++i) {
            if (IsKeyPressed(KEY_ONE + i)) hotbarIndex_ = i;
        }
    }

    void VoxScene::updateDayCycle_(const float dt) {
        dayTime_ += dt / cfg.dayLengthSeconds;
        if (dayTime_ >= 1.0f)
            dayTime_ -= 1.0f;

        // Morning chime as the sun clears the horizon.
        const float sunEl = std::sin(dayTime_ * 2.0f * PI);
        if (sunEl > 0.05f && !chimed_) {
            sfx_.play("chime", 0.5f);
            chimed_ = true;
        }
        if (sunEl < -0.1f)
            chimed_ = false;
    }

    void VoxScene::updateInteraction_(const float dt) {
        const Vector3 eye = player_.eyePosition();
        const Vector3 look = player_.lookDir();
        const auto hit = world_.raycast(eye, look, cfg.reach);
        hasTarget_ = hit.has_value();
        if (hasTarget_)
            target_ = *hit;

        placeTimer_ = std::max(0.0f, placeTimer_ - dt);
        digSoundTimer_ = std::max(0.0f, digSoundTimer_ - dt);

        // --- Breaking (hold LMB) ---
        if (hasTarget_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const BlockInfo& info = blockInfo(target_.block);
            if (info.hardness >= 0.0f) {
                if (target_.x != breakX_ || target_.y != breakY_ || target_.z != breakZ_) {
                    breakX_ = target_.x;
                    breakY_ = target_.y;
                    breakZ_ = target_.z;
                    breakProgress_ = 0.0f;
                }
                const float speed = player_.flying() ? 3.0f : 1.0f; // fly = creative-ish
                breakProgress_ += dt * speed / std::max(info.hardness, 0.05f);
                if (digSoundTimer_ <= 0.0f) {
                    sfx_.play("dig", 0.5f, 1.0f, 0.2f);
                    digSoundTimer_ = 0.22f;
                }
                if (breakProgress_ >= 1.0f) {
                    const Block broken = target_.block;
                    world_.setBlock(target_.x, target_.y, target_.z, Block::Air);
                    sceneEvents().enqueue(BlockBroken{target_.x, target_.y, target_.z, broken});
                    breakProgress_ = 0.0f;
                    breakY_ = -1;
                }
            }
        } else {
            breakProgress_ = 0.0f;
            breakY_ = -1;
        }

        // --- Placing (RMB, with repeat) ---
        if (hasTarget_ && IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && placeTimer_ <= 0.0f) {
            const int px = target_.x + target_.nx;
            const int py = target_.y + target_.ny;
            const int pz = target_.z + target_.nz;
            const Block existing = world_.block(px, py, pz);
            const Block toPlace = kHotbar[static_cast<std::size_t>(hotbarIndex_)];
            if ((existing == Block::Air || existing == Block::Water) &&
                !player_.intersectsBlock(px, py, pz)) {
                world_.setBlock(px, py, pz, toPlace);
                sceneEvents().enqueue(BlockPlaced{px, py, pz, toPlace});
                placeTimer_ = cfg.placeRepeat;
            }
        }
        if (!IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            placeTimer_ = 0.0f;
        }
    }

    void VoxScene::updateDebris_(const float dt) {
        for (auto& d : debris_) {
            d.vel.y -= 14.0f * dt;
            d.pos = Vector3Add(d.pos, Vector3Scale(d.vel, dt));
            d.life -= dt;
        }
        std::erase_if(debris_, [](const Debris& d) { return d.life <= 0.0f; });
    }

    // ------------------------------------------------------------- Light

    float VoxScene::dayLight_() const {
        const float sunEl = std::sin(dayTime_ * 2.0f * PI);
        return std::clamp(0.5f + sunEl * 1.4f, cfg.nightLight, 1.0f);
    }

    Color VoxScene::skyColor_() const {
        const float sunEl = std::sin(dayTime_ * 2.0f * PI);
        const float k = std::clamp((sunEl + 0.12f) / 0.47f, 0.0f, 1.0f);
        Vector3 sky = Vector3Lerp(toV3(pal::skyNight), toV3(pal::skyDay), k);
        const float dusk = std::exp(-std::fabs(sunEl) * 7.0f);
        sky = Vector3Lerp(sky, toV3(pal::skyDusk), dusk * 0.55f);
        return fromV3(sky);
    }

    // -------------------------------------------------------------- Draw

    void VoxScene::draw() {
        Scene::draw();

        // Sky gradient, drawn immediately into the backbuffer: the engine's 3D
        // flush clears depth only, so the world renders on top of this.
        {
            const auto [w, h] = runtime().window().size();
            const Color horizon = skyColor_();
            const Color zenith = fromV3(Vector3Scale(toV3(horizon), 0.45f));
            DrawRectangleGradientV(0, 0, static_cast<int>(w), static_cast<int>(h), zenith, horizon);
        }

        drawSky_();
        drawChunks_();
        drawClouds_();
        if (state_ == State::Playing || state_ == State::Paused) {
            drawHighlight_();
        }
        drawDebris_();
        drawHud_();
    }

    void VoxScene::drawSky_() {
        const float sunAngle = dayTime_ * 2.0f * PI;
        const Vector3 sunDir{std::cos(sunAngle), std::sin(sunAngle), 0.35f};
        const float night = 1.0f - std::clamp(std::sin(sunAngle) * 3.0f + 0.5f, 0.0f, 1.0f);
        const auto stars = starDirs_;
        const Texture2D sunTex = sunTex_;
        const Texture2D moonTex = moonTex_;

        // Celestial bodies write color only; the terrain drawn afterwards
        // depth-tests over them, so mountains correctly occlude the sun.
        rq().submit3D(layers().background(), 0.0f,
                      [sunDir, night, stars, sunTex, moonTex](const Camera3D& cam, const Rectangle&) {
                          rlDisableDepthMask();
                          const Vector3 sunPos = Vector3Add(cam.position, Vector3Scale(Vector3Normalize(sunDir), 340.0f));
                          const Vector3 moonPos = Vector3Add(cam.position, Vector3Scale(Vector3Normalize(sunDir), -340.0f));
                          DrawBillboard(cam, sunTex, sunPos, 42.0f, WHITE);
                          DrawBillboard(cam, moonTex, moonPos, 26.0f, Fade(WHITE, 0.9f));
                          if (night > 0.02f) {
                              for (const auto& dir : stars) {
                                  const Vector3 p = Vector3Add(cam.position, Vector3Scale(dir, 380.0f));
                                  DrawBillboard(cam, sunTex, p, 1.6f, Fade(WHITE, 0.75f * night));
                              }
                          }
                          rlDrawRenderBatchActive(); // flush while the depth mask is still off
                          rlEnableDepthMask();
                      });
    }

    void VoxScene::drawChunks_() {
        const Vector3 camPos = cam3_.position();
        const Vector3 look = Vector3Normalize(Vector3Subtract(cam3_.target(), camPos));
        const bool underwater = player_.eyeInWater() && state_ != State::Menu;
        const float light = dayLight_();

        // Frame uniforms.
        const Vector3 tint = underwater ? Vector3{0.55f * light, 0.75f * light, 1.0f * light}
                                        : Vector3{light, light, light};
        const Vector3 fogColor = underwater ? Vector3Scale(toV3(pal::waterFog), std::max(light, 0.35f))
                                            : toV3(skyColor_());
        const Vector2 fogRange = underwater ? Vector2{2.0f, 24.0f}
                                            : Vector2{cfg.fogStart, cfg.fogEnd};

        SetShaderValue(matLand_.shader, locLandTint_, &tint, SHADER_UNIFORM_VEC3);
        SetShaderValue(matLand_.shader, locLandFogColor_, &fogColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(matLand_.shader, locLandFogRange_, &fogRange, SHADER_UNIFORM_VEC2);
        SetShaderValue(matLand_.shader, locLandCamPos_, &camPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(matWater_.shader, locWaterTint_, &tint, SHADER_UNIFORM_VEC3);
        SetShaderValue(matWater_.shader, locWaterFogColor_, &fogColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(matWater_.shader, locWaterFogRange_, &fogRange, SHADER_UNIFORM_VEC2);
        SetShaderValue(matWater_.shader, locWaterCamPos_, &camPos, SHADER_UNIFORM_VEC3);
        const float t = worldClock_;
        SetShaderValue(matWater_.shader, locWaterTime_, &t, SHADER_UNIFORM_FLOAT);

        drawnChunks_ = 0;
        const float chunkRadius = static_cast<float>(cfg.chunkSize) * 0.9f +
                                  static_cast<float>(cfg.worldHeight) * 0.5f;
        const float maxDist = static_cast<float>((cfg.viewRadius + 1) * cfg.chunkSize);

        for (auto& [key, chunk] : world_.chunks()) {
            if (!chunk.hasMesh)
                continue;
            const Vector3 center{(static_cast<float>(key.cx) + 0.5f) * cfg.chunkSize,
                                 static_cast<float>(cfg.worldHeight) * 0.5f,
                                 (static_cast<float>(key.cz) + 0.5f) * cfg.chunkSize};
            const Vector3 toChunk = Vector3Subtract(center, camPos);
            const float horizDist = std::sqrt(toChunk.x * toChunk.x + toChunk.z * toChunk.z);
            if (horizDist > maxDist + chunkRadius)
                continue;
            // Cheap cone cull: skip chunks fully behind the camera.
            if (horizDist > chunkRadius && Vector3DotProduct(toChunk, look) < -chunkRadius)
                continue;

            drawnChunks_++;
            if (chunk.opaqueMesh.vertexCount > 0) {
                const Mesh mesh = chunk.opaqueMesh;
                const Material mat = matLand_;
                rq().submit3D(layers().world(), 0.0f, [mesh, mat](const Camera3D&, const Rectangle&) {
                    DrawMesh(mesh, mat, MatrixIdentity());
                });
            }
            if (chunk.waterMesh.vertexCount > 0) {
                const Mesh mesh = chunk.waterMesh;
                const Material mat = matWater_;
                rq().submit3D(layers().foreground(), 0.0f, [mesh, mat](const Camera3D&, const Rectangle&) {
                    DrawMesh(mesh, mat, MatrixIdentity());
                });
            }
        }
    }

    void VoxScene::drawHighlight_() {
        if (!hasTarget_)
            return;
        const Vector3 center{target_.x + 0.5f, target_.y + 0.5f, target_.z + 0.5f};
        const float progress = breakProgress_;
        rq().submit3D(layers().foreground(), 1.0f, [center, progress](const Camera3D&, const Rectangle&) {
            DrawCubeWiresV(center, {1.004f, 1.004f, 1.004f}, Fade(BLACK, 0.75f));
            if (progress > 0.0f) {
                const float s = 1.0f - progress * 0.75f;
                DrawCubeWiresV(center, {s, s, s}, Fade(WHITE, 0.9f));
                DrawCubeV(center, {1.002f, 1.002f, 1.002f}, Fade(BLACK, 0.25f * progress));
            }
        });
    }

    void VoxScene::drawDebris_() {
        if (debris_.empty())
            return;
        const auto debris = debris_;
        rq().submit3D(layers().foreground(), 2.0f, [debris](const Camera3D&, const Rectangle&) {
            for (const auto& d : debris) {
                DrawCubeV(d.pos, {d.size, d.size, d.size}, Fade(d.color, std::min(1.0f, d.life * 3.0f)));
            }
        });
    }

    void VoxScene::drawClouds_() {
        const float light = dayLight_();
        const Vector3 camPos = cam3_.position();
        const float drift = worldClock_ * 1.6f;
        const auto clouds = clouds_;
        rq().submit3D(layers().foreground(), 3.0f, [clouds, light, camPos, drift](const Camera3D&, const Rectangle&) {
            constexpr float tile = 480.0f;
            const Color base = Fade(fromV3({light, light, light}), 1.0f);
            for (const auto& c : clouds) {
                float x = std::fmod(c.pos.x + drift - camPos.x, tile);
                if (x < 0.0f) x += tile;
                float z = std::fmod(c.pos.y - camPos.z, tile);
                if (z < 0.0f) z += tile;
                const Vector3 p{camPos.x + x - tile * 0.5f, 78.0f, camPos.z + z - tile * 0.5f};
                DrawCubeV(p, {c.w, 2.2f, c.d}, Fade(base, c.alpha));
            }
        });
    }

    // ---------------------------------------------------------------- HUD

    void VoxScene::drawHud_() {
        const View* view = primaryView();
        if (!view)
            return;
        const Rectangle vp = view->viewport;

        const State state = state_;
        const bool underwater = player_.eyeInWater() && state != State::Menu;
        const float breakProgress = breakProgress_;
        const int hotbarIndex = hotbarIndex_;
        const Texture2D atlas = atlas_;
        const Vector3 pos = player_.position();
        const bool flying = player_.flying();
        const int chunksLoaded = static_cast<int>(world_.chunks().size());
        const int chunksDrawn = drawnChunks_;
        const float clock = std::fmod(6.0f + dayTime_ * 24.0f, 24.0f);
        const float time = worldClock_;
        const Block targetBlock = hasTarget_ ? target_.block : Block::Air;

        rq().submitUI([=] {
            const float cx = vp.x + vp.width * 0.5f;
            const float cy = vp.y + vp.height * 0.5f;

            if (underwater) {
                DrawRectangleRec(vp, Fade(Color{30, 70, 160, 255}, 0.3f));
            }

            if (state == State::Menu) {
                // Title card over the orbiting world.
                DrawRectangleRec(vp, Fade(BLACK, 0.28f));
                const char* title = "VOXHAVEN";
                const float titleSize = 96.0f;
                const Vector2 ext = measure(title, titleSize);
                const float bob = std::sin(time * 1.4f) * 6.0f;
                drawLabel({cx - ext.x * 0.5f + 4, vp.y + vp.height * 0.2f + bob + 4}, title, titleSize,
                          Fade(BLACK, 0.6f));
                drawLabel({cx - ext.x * 0.5f, vp.y + vp.height * 0.2f + bob}, title, titleSize,
                          Color{255, 226, 130, 255});
                const char* sub = "AN RLGE VOXEL SANDBOX";
                const Vector2 subExt = measure(sub, 20);
                drawLabel({cx - subExt.x * 0.5f, vp.y + vp.height * 0.2f + bob + ext.y + 8}, sub, 20,
                          pal::hudDim);

                if (std::fmod(time, 1.0f) < 0.66f) {
                    const char* prompt = "PRESS ENTER OR SPACE TO ENTER THE WORLD";
                    const Vector2 pExt = measure(prompt, 26);
                    drawLabel({cx - pExt.x * 0.5f, vp.y + vp.height * 0.62f}, prompt, 26, pal::hudText);
                }

                const char* lines[4] = {
                    "WASD - MOVE      MOUSE - LOOK      SPACE - JUMP      CTRL - SPRINT",
                    "LMB - MINE       RMB - PLACE       WHEEL / 1-9 - SELECT BLOCK",
                    "F - FLY          T - FAST-FORWARD TIME       ESC - PAUSE",
                    "WORLD AND EDITS AUTOSAVE TO voxhaven.world",
                };
                float ly = vp.y + vp.height * 0.72f;
                for (const auto* line : lines) {
                    const Vector2 lExt = measure(line, 15);
                    drawLabel({cx - lExt.x * 0.5f, ly}, line, 15, pal::hudDim);
                    ly += 24.0f;
                }
                return;
            }

            // --- Crosshair + break progress ---
            DrawLineEx({cx - 9, cy}, {cx + 9, cy}, 2.0f, Fade(WHITE, 0.8f));
            DrawLineEx({cx, cy - 9}, {cx, cy + 9}, 2.0f, Fade(WHITE, 0.8f));
            if (breakProgress > 0.0f) {
                DrawRing({cx, cy}, 14.0f, 18.0f, -90.0f, -90.0f + 360.0f * breakProgress, 32,
                         Fade(pal::hudAccent, 0.9f));
            }

            // --- Hotbar ---
            constexpr float slot = 48.0f;
            const float barW = slot * 9.0f;
            const float barX = cx - barW * 0.5f;
            const float barY = vp.y + vp.height - slot - 14.0f;
            for (int i = 0; i < 9; ++i) {
                const Rectangle r{barX + i * slot, barY, slot, slot};
                DrawRectangleRec(r, Fade(BLACK, i == hotbarIndex ? 0.6f : 0.42f));
                const Block b = kHotbar[static_cast<std::size_t>(i)];
                const BlockInfo& info = blockInfo(b);
                const Rectangle uv = tileUV(info.tileSide);
                const Rectangle src{uv.x * atlas.width, uv.y * atlas.height,
                                    uv.width * atlas.width, uv.height * atlas.height};
                const Rectangle dst{r.x + 8, r.y + 8, slot - 16, slot - 16};
                DrawTexturePro(atlas, src, dst, {0, 0}, 0.0f, WHITE);
                DrawRectangleLinesEx(r, i == hotbarIndex ? 3.0f : 1.0f,
                                     i == hotbarIndex ? pal::hudAccent : Fade(WHITE, 0.35f));
                drawLabel({r.x + 4, r.y + 2}, TextFormat("%d", i + 1), 12, Fade(WHITE, 0.5f));
            }
            {
                const BlockInfo& sel = blockInfo(kHotbar[static_cast<std::size_t>(hotbarIndex)]);
                const Vector2 nExt = measure(sel.name, 18);
                drawLabel({cx - nExt.x * 0.5f, barY - 26}, sel.name, 18, pal::hudText);
            }

            // --- Info line ---
            const int hours = static_cast<int>(clock);
            const int minutes = static_cast<int>((clock - hours) * 60.0f);
            drawLabel({vp.x + 14, vp.y + 12},
                      TextFormat("%2d FPS   XYZ %.1f / %.1f / %.1f   %02d:%02d   chunks %d (drawn %d)%s",
                                 GetFPS(), pos.x, pos.y, pos.z, hours, minutes, chunksLoaded,
                                 chunksDrawn, flying ? "   [FLY]" : ""),
                      14, Fade(pal::hudText, 0.75f));
            if (targetBlock != Block::Air) {
                drawLabel({vp.x + 14, vp.y + 32}, TextFormat("looking at: %s", blockInfo(targetBlock).name),
                          14, Fade(pal::hudDim, 0.8f));
            }

            // --- Pause overlay ---
            if (state == State::Paused) {
                DrawRectangleRec(vp, Fade(BLACK, 0.55f));
                const Vector2 ext = measure("PAUSED", 64);
                drawLabel({cx - ext.x * 0.5f, vp.y + vp.height * 0.4f}, "PAUSED", 64, pal::hudText);
                const char* hint = "ESC / P - RESUME      Q - SAVE AND QUIT";
                const Vector2 hExt = measure(hint, 18);
                drawLabel({cx - hExt.x * 0.5f, vp.y + vp.height * 0.4f + 84}, hint, 18, pal::hudDim);
            }
        });
    }

    // -------------------------------------------------------------- Debug

    void VoxScene::debugOverlay() {
        if (ImGui::Begin("Voxhaven")) {
            ImGui::Text("state %d  chunks %zu  pendingMesh %d  drawn %d", static_cast<int>(state_),
                        world_.chunks().size(), world_.pendingMeshes(), drawnChunks_);
            ImGui::Text("pos %.1f %.1f %.1f  ground %d  water %d", player_.position().x,
                        player_.position().y, player_.position().z, player_.onGround(),
                        player_.inWater());
            ImGui::SliderFloat("time of day", &dayTime_, 0.0f, 0.999f);
            bool fly = player_.flying();
            if (ImGui::Checkbox("fly", &fly)) {
                player_.toggleFly();
            }
            if (ImGui::Button("save world")) {
                world_.save();
            }
            const auto& stats = rq().stats();
            ImGui::Separator();
            ImGui::Text("3D cmds %zu  executed %zu  flush %.2fms", stats.customCommands3D,
                        stats.executedDrawCalls3D, stats.flushTimeMs);
        }
        ImGui::End();
    }

} // namespace vox
