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
#include "vx_worldgen.hpp"
#include "vx_postfx.hpp"

namespace vox {
    using namespace rlge;

    namespace {
        constexpr auto kLandVertexShader = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
out vec2 fragTexCoord;
out vec2 fragTileOrigin;
out vec4 fragColor;
out vec3 fragPosition;
void main() {
    fragTexCoord = vertexTexCoord;
    fragTileOrigin = vertexTexCoord2;
    fragColor = vertexColor;
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

        constexpr auto kWaterVertexShader = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform float u_time;
out vec2 fragTexCoord;
out vec2 fragTileOrigin;
out vec4 fragColor;
out vec3 fragPosition;
void main() {
    vec3 pos = vertexPosition;
    pos.y += sin(u_time * 1.6 + vertexPosition.x * 0.9 + vertexPosition.z * 0.7) * 0.05;
    fragTexCoord = vertexTexCoord;
    fragTileOrigin = vertexTexCoord2;
    fragColor = vertexColor;
    fragPosition = vec3(matModel * vec4(pos, 1.0));
    gl_Position = mvp * vec4(pos, 1.0);
}
)";

        // Vertex colors carry the light model baked by the mesher:
        //   R = ambient occlusion * directional face shade
        //   G = skylight   (0..1)   B = blocklight (0..1)
        // Combining them here means the day/night cycle only moves a uniform -
        // no chunk ever needs remeshing as the sun moves.
        constexpr auto kChunkFragmentShader = R"(
#version 330
in vec2 fragTexCoord;
in vec2 fragTileOrigin;
in vec4 fragColor;
in vec3 fragPosition;
uniform sampler2D texture0;
uniform vec3 u_tint;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform vec3 u_camPos;
uniform float u_dayFactor;
uniform float u_ambient;
out vec4 finalColor;
const vec2 kTile = vec2(1.0 / 8.0, 1.0 / 4.0);

void main() {
    // Greedy meshing lets UVs run past the tile, so wrap them back into the
    // quad's own atlas tile before sampling.
    vec2 uv = fragTileOrigin + mod(fragTexCoord - fragTileOrigin, kTile);
    vec4 tex = texture(texture0, uv);
    if (tex.a < 0.35) discard;

    float ao    = fragColor.r;
    float sky   = fragColor.g * u_dayFactor;
    float block = fragColor.b;

    // Light levels are interpolated linearly (as levels), then converted to
    // brightness with a per-level falloff. A linear ramp washes everything out
    // and leaves overhangs barely darker than open sky; this gives canopies,
    // cave mouths and doorways real depth.
    float level = max(sky, block);
    float lum   = max(pow(0.84, (1.0 - level) * 15.0), u_ambient);

    // Torchlight reads warm where it wins over daylight.
    vec3 warm  = vec3(1.0, 0.87, 0.68);
    vec3 hue   = mix(vec3(1.0), warm, clamp(block - sky, 0.0, 1.0));

    vec3 col = tex.rgb * u_tint * hue * (lum * ao);
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
        setHudFonts(Font{}, Font{}); // drop the HUD's borrowed handles first
        if (uiFont_.texture.id != 0) UnloadFont(uiFont_);
        if (displayFont_.texture.id != 0) UnloadFont(displayFont_);
        if (atlas_.id != 0) UnloadTexture(atlas_);
        if (sunTex_.id != 0) UnloadTexture(sunTex_);
        if (moonTex_.id != 0) UnloadTexture(moonTex_);
    }

    void VoxScene::enter() {
        SetExitKey(KEY_NULL); // ESC pauses; quitting goes through the pause menu
        atlas_ = buildAtlas(world_.seed());
        sunTex_ = makeDiscTexture(Color{255, 236, 160, 255}, Color{255, 190, 90, 255}, false);
        moonTex_ = makeDiscTexture(Color{224, 228, 240, 255}, Color{160, 168, 190, 255}, true);

        setupFonts_();
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

        const Vector2 spawnXZ = findSpawnPoint_();
        spawnPos_ = {spawnXZ.x, spawnPos_.y, spawnXZ.y};
        warmStart_();
        player_.spawn(world_, spawnPos_.x, spawnPos_.z);
        spawnPos_ = player_.position();

        mobs_.init(world_.seed() ^ 0x9E3779B9u);
        inventory_.giveStarterKit();
        titleMenu_.open(worldClock_);
        EnableCursor();

        // Feedback effects ride the scene event bus, not the edit code path.
        sceneEvents().subscribe<BlockBroken>([this](const BlockBroken& e) {
            const Color c = blockInfo(e.block).mapColor;
            for (int i = 0; i < 14; ++i) {
                debris_.push_back({{e.x + 0.5f, e.y + 0.5f, e.z + 0.5f},
                                   {(frand01() - 0.5f) * 5.0f, 2.0f + frand01() * 4.0f,
                                    (frand01() - 0.5f) * 5.0f},
                                   c, 0.5f + frand01() * 0.4f, 0.07f + frand01() * 0.08f});
            }
            sfx_.playBreak(blockInfo(e.block).sound);
        });
        sceneEvents().subscribe<BlockPlaced>([this](const BlockPlaced& e) {
            sfx_.playPlace(blockInfo(e.block).sound);
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
        postFx_.shutdown();
        mobs_.shutdown();
        world_.save();
        EnableCursor();
    }

    RenderTexture2D* VoxScene::beginWorldRenderTarget() {
        const Vector2 size = runtime().window().renderSize();
        return postFx_.target(static_cast<int>(size.x), static_cast<int>(size.y));
    }

    void VoxScene::afterWorldRender(RenderTexture2D* target, const std::vector<View>&) {
        if (target == nullptr)
            return;
        const Vector2 size = runtime().window().renderSize();
        postFx_.apply({0.0f, 0.0f, size.x, size.y}, worldClock_,
                      player_.eyeInWater() && state_ != State::Menu, dayLight_());
    }

    void VoxScene::setupFonts_() {
        // Glyphs are rasterized once at a high base size and scaled down, which
        // keeps HUD text crisp at every window size. If the files are missing the
        // HUD silently falls back to raylib's built-in font.
        static constexpr int kBaseSize = 96;
        const char* roots[] = {
            "../examples/voxhaven/assets/",
            "examples/voxhaven/assets/",
            "../../examples/voxhaven/assets/",
            "assets/",
        };
        const auto tryLoad = [&](const char* file) {
            Font f{};
            for (const char* root : roots) {
                const char* path = TextFormat("%s%s", root, file);
                if (!FileExists(path))
                    continue;
                f = LoadFontEx(path, kBaseSize, nullptr, 0);
                if (f.texture.id != 0) {
                    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
                    TraceLog(LOG_INFO, "VOXHAVEN: loaded font %s", path);
                    break;
                }
            }
            return f;
        };

        uiFont_ = tryLoad("rubik_ui.ttf");
        displayFont_ = tryLoad("rubik_mono.ttf");
        setHudFonts(uiFont_, displayFont_);
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
        locLandDay_ = GetShaderLocation(land, "u_dayFactor");
        locLandAmbient_ = GetShaderLocation(land, "u_ambient");
        locWaterTint_ = GetShaderLocation(water, "u_tint");
        locWaterFogColor_ = GetShaderLocation(water, "u_fogColor");
        locWaterFogRange_ = GetShaderLocation(water, "u_fogRange");
        locWaterCamPos_ = GetShaderLocation(water, "u_camPos");
        locWaterTime_ = GetShaderLocation(water, "u_time");
        locWaterDay_ = GetShaderLocation(water, "u_dayFactor");
        locWaterAmbient_ = GetShaderLocation(water, "u_ambient");

        matLand_ = LoadMaterialDefault();
        matLand_.shader = land;
        matLand_.maps[MATERIAL_MAP_ALBEDO].texture = atlas_;
        matWater_ = LoadMaterialDefault();
        matWater_.shader = water;
        matWater_.maps[MATERIAL_MAP_ALBEDO].texture = atlas_;
        materialsReady_ = true;

        postFx_.init(store);
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

    Vector2 VoxScene::findSpawnPoint_() const {
        // worldgen is a pure function of (seed, x, z), so we can probe columns
        // for dry land long before any chunk is generated. Without this a seed
        // whose origin lands in an ocean drops the player into open water with
        // nothing to stand on and no animals anywhere in range.
        const std::uint32_t seed = world_.seed();
        constexpr int kStep = 12;
        constexpr int kMaxRing = 60; // ~720 blocks out
        for (int ring = 0; ring <= kMaxRing; ++ring) {
            for (int dz = -ring; dz <= ring; ++dz) {
                for (int dx = -ring; dx <= ring; ++dx) {
                    if (ring > 0 && std::max(std::abs(dx), std::abs(dz)) != ring)
                        continue; // perimeter only, so we spiral outwards
                    const int x = dx * kStep;
                    const int z = dz * kStep;
                    const Biome b = worldgen::biomeAt(seed, x, z);
                    if (b == Biome::Ocean || b == Biome::Beach)
                        continue;
                    const int h = worldgen::columnHeight(seed, x, z);
                    if (h <= cfg.seaLevel + 2)
                        continue;
                    return {static_cast<float>(x) + 0.5f, static_cast<float>(z) + 0.5f};
                }
            }
        }
        return {8.5f, 8.5f};
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
        if (cursorFreeState_()) {
            EnableCursor();
        } else {
            DisableCursor();
            skipLookFrames_ = 2; // swallow the jump the OS reports on cursor capture
        }
        if (s == State::Menu)
            titleMenu_.open(worldClock_);
        else if (s == State::Paused)
            pauseMenu_.open(worldClock_);
        else if (s == State::Inventory)
            inventoryUi_.open(worldClock_);
    }

    bool VoxScene::cursorFreeState_() const { return state_ != State::Playing; }

    // ------------------------------------------------------------- Update

    void VoxScene::update(const float dt) {
        Scene::update(dt);
        worldClock_ += dt;
        inventory_.syncCreative();

        // FPS ring buffer for the HUD sparkline.
        fpsSamples_[fpsHead_] = static_cast<float>(GetFPS());
        fpsHead_ = (fpsHead_ + 1) % 64;

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
            break;
        }
        case State::Playing:
            updatePlaying_(dt);
            break;
        case State::Paused:
        case State::Settings:
            break; // menus drive themselves; actions are consumed below
        case State::Inventory:
            if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ESCAPE)) {
                returnHeldToInventory(inventory_, inventoryUi_);
                setState_(State::Playing);
            }
            break;
        }

        // Menu widgets record their action while drawing, so it is consumed on
        // the following frame.
        updateMenuActions_();
        updateDebris_(dt);
    }

    void VoxScene::updateMenuActions_() {
        switch (state_) {
        case State::Menu:
            switch (titleMenu_.take()) {
            case MenuAction::Play: setState_(State::Playing); break;
            case MenuAction::Settings:
                settingsReturn_ = State::Menu;
                setState_(State::Settings);
                break;
            case MenuAction::Quit: runtime().quit(); break;
            default: break;
            }
            break;
        case State::Paused:
            switch (pauseMenu_.take()) {
            case MenuAction::Resume: setState_(State::Playing); break;
            case MenuAction::Settings:
                settingsReturn_ = State::Paused;
                setState_(State::Settings);
                break;
            case MenuAction::SaveAndQuit:
                world_.save();
                runtime().quit();
                break;
            default: break;
            }
            break;
        case State::Settings:
            if (settingsUi_.take() == MenuAction::Back) {
                setState_(settingsReturn_);
            }
            break;
        default:
            break;
        }
    }

    void VoxScene::updatePlaying_(const float dt) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
            setState_(State::Paused);
            return;
        }
        if (IsKeyPressed(KEY_E)) {
            setState_(State::Inventory);
            return;
        }
        if (IsKeyPressed(KEY_F) && settings.creative) {
            player_.toggleFly();
        }
        if (IsKeyPressed(KEY_M) && settings.creative) {
            mobs_.spawnSampler(world_, player_.position(), player_.yaw());
        }
        if (IsKeyPressed(KEY_G)) {
            // Quick creative toggle; leaving creative also drops you out of fly.
            settings.creative = !settings.creative;
            if (!settings.creative)
                player_.setFly(false);
        }

        // Gather inputs (engine axis bindings + raw keys).
        PlayerController::Inputs in;
        in.moveX = input().axisValue(Action::MoveRight);
        in.moveZ = -input().axisValue(Action::MoveDown); // W = forward
        in.jump = IsKeyDown(KEY_SPACE);
        in.jumpPressed = IsKeyPressed(KEY_SPACE);
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
        if (player_.justStepped()) sfx_.playFootstep(player_.groundSound());
        if (player_.justLanded()) {
            sfx_.play("land", 0.2f + 0.5f * player_.landingImpact(), 1.0f, 0.1f);
        }

        // Camera follows the eye; sprinting widens the FOV a touch for speed.
        const Vector3 eye = player_.eyePosition();
        const Vector3 look = player_.lookDir();
        cam3_.setPosition(eye);
        cam3_.setTarget(Vector3Add(eye, look));
        const float targetFov = settings.fov + (player_.sprinting() ? 6.0f : 0.0f) *
                                                   player_.speedFraction();
        cam3_.setFovy(cam3_.fovy() + (targetFov - cam3_.fovy()) * std::min(1.0f, dt * 6.0f));

        // World streaming + meshing budget.
        world_.update(player_.position());
        meshedThisFrame_ = Mesher::remeshDirty(world_, player_.position(), cfg.meshPerFrame);

        updateDayCycle_(IsKeyDown(KEY_T) ? dt * 40.0f : dt);

        mobs_.update(world_, player_.position(), dayTime_, dt);
        updateCombat_(dt);
        updateInteraction_(dt);

        // Hotbar selection.
        const float wheel = GetMouseWheelMove();
        const int before = inventory_.selected();
        if (wheel < -0.1f) inventory_.scrollSelection(1);
        if (wheel > 0.1f) inventory_.scrollSelection(-1);
        for (int i = 0; i < 9; ++i) {
            if (IsKeyPressed(KEY_ONE + i)) inventory_.select(i);
        }
        if (inventory_.selected() != before)
            selectionChangedAt_ = worldClock_;
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
        if (hasTarget_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && swingCooldown_ <= 0.0f) {
            const BlockInfo& info = blockInfo(target_.block);
            if (info.hardness >= 0.0f) {
                if (target_.x != breakX_ || target_.y != breakY_ || target_.z != breakZ_) {
                    breakX_ = target_.x;
                    breakY_ = target_.y;
                    breakZ_ = target_.z;
                    breakProgress_ = 0.0f;
                }
                const float speed = settings.creative ? 6.0f : 1.0f;
                breakProgress_ += dt * speed / std::max(info.hardness, 0.05f);
                if (digSoundTimer_ <= 0.0f) {
                    sfx_.playDig(info.sound);
                    digSoundTimer_ = 0.22f;
                }
                if (breakProgress_ >= 1.0f) {
                    const Block broken = target_.block;
                    world_.setBlock(target_.x, target_.y, target_.z, Block::Air);
                    // Mining yields the block's drop; a full inventory simply
                    // loses it rather than blocking the dig.
                    if (const Block drop = blockInfo(broken).drop; drop != Block::Air) {
                        inventory_.add(drop, 1);
                    }
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
            const Block toPlace = inventory_.selectedBlock();
            if (toPlace != Block::Air && (existing == Block::Air || existing == Block::Water) &&
                !player_.intersectsBlock(px, py, pz) && inventory_.consumeSelected(1)) {
                world_.setBlock(px, py, pz, toPlace);
                sceneEvents().enqueue(BlockPlaced{px, py, pz, toPlace});
                placeTimer_ = cfg.placeRepeat;
            }
        }
        if (!IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            placeTimer_ = 0.0f;
        }
    }

    void VoxScene::updateCombat_(const float dt) {
        swingCooldown_ = std::max(0.0f, swingCooldown_ - dt);
        hurtFlash_ = std::max(0.0f, hurtFlash_ - dt * 2.0f);

        // Mob deaths become debris and a thud, reusing the block-break feedback.
        for (const auto& [pos, kind] : mobs_.deaths) {
            const Color c = mobColor(kind);
            for (int i = 0; i < 18; ++i) {
                debris_.push_back({pos,
                                   {(frand01() - 0.5f) * 5.0f, 1.5f + frand01() * 4.0f,
                                    (frand01() - 0.5f) * 5.0f},
                                   c, 0.5f + frand01() * 0.5f, 0.07f + frand01() * 0.07f});
            }
            sfx_.play("break", 0.55f, mobStats(kind).hostile ? 0.7f : 1.15f, 0.15f);
        }

        // Damage dealt to the player by everything that reached them this frame.
        if (const int dmg = mobs_.takePlayerDamage(); dmg > 0 && !settings.creative && deathTimer_ <= 0.0f) {
            playerHealth_ = std::max(0, playerHealth_ - dmg);
            hurtFlash_ = 1.0f;
            regenTimer_ = 0.0f;
            sfx_.play("hurt", 0.7f, 1.0f, 0.1f);
            if (playerHealth_ == 0) {
                deathTimer_ = 2.4f;
                sfx_.play("land", 0.9f, 0.6f);
            }
        }

        // Slow regeneration once nothing has hit you for a while.
        if (playerHealth_ > 0 && playerHealth_ < playerMaxHealth_) {
            regenTimer_ += dt;
            if (regenTimer_ > 4.0f) {
                regenTimer_ = 0.0f;
                playerHealth_ = std::min(playerMaxHealth_, playerHealth_ + 1);
            }
        }

        if (deathTimer_ > 0.0f) {
            deathTimer_ -= dt;
            if (deathTimer_ <= 0.0f)
                respawnPlayer_();
            return;
        }

        // Swinging at a creature takes priority over mining the block behind it.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && swingCooldown_ <= 0.0f) {
            const int damage = settings.creative ? 40 : 4;
            if (mobs_.attack(player_.eyePosition(), player_.lookDir(), cfg.reach, damage)) {
                swingCooldown_ = 0.35f;
                sfx_.playDig(SoundGroup::Wood, 0.45f);
            }
        }
    }

    void VoxScene::respawnPlayer_() {
        playerHealth_ = playerMaxHealth_;
        regenTimer_ = 0.0f;
        hurtFlash_ = 0.0f;
        player_.spawn(world_, spawnPos_.x, spawnPos_.z);
        mobs_.clear(); // give the player a clean slate at the spawn point
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

        drawSky_();
        drawChunks_();
        drawClouds_();
        if (state_ == State::Playing || state_ == State::Paused) {
            drawHighlight_();
        }
        drawMobs_();
        drawDebris_();
        drawUi_();
    }

    void VoxScene::drawSky_() {
        const Color horizon = skyColor_();
        const Color zenith = fromV3(Vector3Scale(toV3(horizon), 0.45f));
        rq().submit3D(layers().background(), -1.0f,
                      [horizon, zenith](const Camera3D& cam, const Rectangle&) {
                          // Untextured gradient shell locked to the camera. Drawn
                          // with the depth mask off so everything else overwrites it.
                          rlDisableDepthMask();
                          rlDisableBackfaceCulling();
                          rlSetTexture(rlGetTextureIdDefault());
                          rlBegin(RL_QUADS);
                          constexpr float r = 400.0f;
                          constexpr float top = 300.0f;
                          constexpr float bot = -300.0f;
                          const Vector3 c = cam.position;
                          const float sx[4][4] = {{-r, -r, r, -r}, {r, -r, r, r},
                                                  {r, r, -r, r},   {-r, r, -r, -r}};
                          for (const auto& q : sx) {
                              rlColor4ub(horizon.r, horizon.g, horizon.b, 255);
                              rlVertex3f(c.x + q[0], c.y + bot, c.z + q[1]);
                              rlVertex3f(c.x + q[2], c.y + bot, c.z + q[3]);
                              rlColor4ub(zenith.r, zenith.g, zenith.b, 255);
                              rlVertex3f(c.x + q[2], c.y + top, c.z + q[3]);
                              rlVertex3f(c.x + q[0], c.y + top, c.z + q[1]);
                          }
                          rlColor4ub(zenith.r, zenith.g, zenith.b, 255);
                          rlVertex3f(c.x - r, c.y + top, c.z - r);
                          rlVertex3f(c.x - r, c.y + top, c.z + r);
                          rlVertex3f(c.x + r, c.y + top, c.z + r);
                          rlVertex3f(c.x + r, c.y + top, c.z - r);
                          rlColor4ub(horizon.r, horizon.g, horizon.b, 255);
                          rlVertex3f(c.x - r, c.y + bot, c.z - r);
                          rlVertex3f(c.x + r, c.y + bot, c.z - r);
                          rlVertex3f(c.x + r, c.y + bot, c.z + r);
                          rlVertex3f(c.x - r, c.y + bot, c.z + r);
                          rlEnd();
                          rlSetTexture(0);
                          rlDrawRenderBatchActive();
                          rlEnableBackfaceCulling();
                          rlEnableDepthMask();
                      });

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

        // Frame uniforms. u_tint is pure color now; brightness comes from the
        // baked sky/block light combined with u_dayFactor in the shader.
        const Vector3 tint = underwater ? Vector3{0.55f, 0.75f, 1.0f} : Vector3{1.0f, 1.0f, 1.0f};
        const Vector3 fogColor = underwater ? Vector3Scale(toV3(pal::waterFog), std::max(light, 0.35f))
                                            : toV3(skyColor_());
        const Vector2 fogRange = underwater ? Vector2{2.0f, 24.0f}
                                            : Vector2{settings.fogStart(), settings.fogEnd()};
        const float dayFactor = light;
        // A faint floor keeps unlit caves navigable rather than pure black.
        const float ambient = 0.045f;

        SetShaderValue(matLand_.shader, locLandTint_, &tint, SHADER_UNIFORM_VEC3);
        SetShaderValue(matLand_.shader, locLandFogColor_, &fogColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(matLand_.shader, locLandFogRange_, &fogRange, SHADER_UNIFORM_VEC2);
        SetShaderValue(matLand_.shader, locLandCamPos_, &camPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(matLand_.shader, locLandDay_, &dayFactor, SHADER_UNIFORM_FLOAT);
        SetShaderValue(matLand_.shader, locLandAmbient_, &ambient, SHADER_UNIFORM_FLOAT);
        SetShaderValue(matWater_.shader, locWaterTint_, &tint, SHADER_UNIFORM_VEC3);
        SetShaderValue(matWater_.shader, locWaterFogColor_, &fogColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(matWater_.shader, locWaterFogRange_, &fogRange, SHADER_UNIFORM_VEC2);
        SetShaderValue(matWater_.shader, locWaterCamPos_, &camPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(matWater_.shader, locWaterDay_, &dayFactor, SHADER_UNIFORM_FLOAT);
        SetShaderValue(matWater_.shader, locWaterAmbient_, &ambient, SHADER_UNIFORM_FLOAT);
        const float t = worldClock_;
        SetShaderValue(matWater_.shader, locWaterTime_, &t, SHADER_UNIFORM_FLOAT);

        drawnChunks_ = 0;
        visibleOpaque_.clear();
        visibleWater_.clear();

        const float chunkRadius = static_cast<float>(cfg.chunkSize);
        const float maxDist = static_cast<float>((settings.viewRadius + 1) * cfg.chunkSize);
        const View* view = primaryView();
        const float aspect = (view && view->viewport.height > 0.0f)
                                 ? view->viewport.width / view->viewport.height
                                 : 16.0f / 9.0f;
        const Camera3D& cam = cam3_.cam3d();

        for (auto& [key, chunk] : world_.chunks()) {
            if (!chunk.hasMesh || chunk.emptyColumn())
                continue;
            const Vector3 center{(static_cast<float>(key.cx) + 0.5f) * cfg.chunkSize,
                                 static_cast<float>(cfg.worldHeight) * 0.5f,
                                 (static_cast<float>(key.cz) + 0.5f) * cfg.chunkSize};
            const Vector3 toChunk = Vector3Subtract(center, camPos);
            const float horizDist = std::sqrt(toChunk.x * toChunk.x + toChunk.z * toChunk.z);
            if (horizDist > maxDist + chunkRadius)
                continue;
            // Proper 6-plane frustum test against the chunk's tight AABB.
            if (horizDist > chunkRadius && !chunkInFrustum(cam, aspect, chunk.bounds()))
                continue;

            drawnChunks_++;
            if (chunk.opaqueMesh.vertexCount > 0)
                visibleOpaque_.push_back(chunk.opaqueMesh);
            if (chunk.waterMesh.vertexCount > 0)
                visibleWater_.push_back(chunk.waterMesh);
        }

        // One command per layer, iterating the gathered lists at flush time.
        // The lists stay alive and untouched between submit and flush.
        if (!visibleOpaque_.empty()) {
            rq().submit3D(layers().world(), 0.0f, [this](const Camera3D&, const Rectangle&) {
                for (const Mesh& mesh : visibleOpaque_) {
                    DrawMesh(mesh, matLand_, MatrixIdentity());
                }
            });
        }
        if (!visibleWater_.empty()) {
            rq().submit3D(layers().foreground(), 0.0f, [this](const Camera3D&, const Rectangle&) {
                for (const Mesh& mesh : visibleWater_) {
                    DrawMesh(mesh, matWater_, MatrixIdentity());
                }
            });
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

    void VoxScene::drawMobs_() {
        const float light = dayLight_();
        rq().submit3D(layers().world(), 1.0f, [this, light](const Camera3D& cam, const Rectangle&) {
            mobs_.draw(cam, light);
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

    HudContext VoxScene::makeHudContext_() {
        HudContext ctx{};
        if (const View* view = primaryView())
            ctx.viewport = view->viewport;
        ctx.inventory = &inventory_;
        ctx.atlas = atlas_;
        ctx.time = worldClock_;
        ctx.dayFraction = dayTime_;
        ctx.fps = GetFPS();
        ctx.playerPos = player_.position();
        ctx.biomeName = biomeName(worldgen::biomeAt(world_.seed(),
                                                    static_cast<int>(std::floor(player_.position().x)),
                                                    static_cast<int>(std::floor(player_.position().z))));
        ctx.underwater = player_.eyeInWater() && state_ != State::Menu;
        ctx.flying = player_.flying();
        ctx.creative = settings.creative;
        ctx.sprinting = player_.sprinting();
        ctx.onGround = player_.onGround();
        ctx.health = playerHealth_;
        ctx.maxHealth = playerMaxHealth_;
        ctx.showHealth = true; // mobs can hurt you, so hearts matter now
        ctx.breakProgress = breakProgress_;
        ctx.hasTarget = hasTarget_;
        ctx.lookingAtName = hasTarget_ ? blockInfo(target_.block).name : nullptr;
        ctx.chunksLoaded = static_cast<int>(world_.chunks().size());
        ctx.chunksDrawn = drawnChunks_;
        ctx.meshedThisFrame = meshedThisFrame_;
        for (int i = 0; i < 64; ++i)
            ctx.fpsGraphSamples[i] = fpsSamples_[i];
        ctx.fpsGraphHead = fpsHead_;
        ctx.selectionChangedAt = selectionChangedAt_;
        ctx.versionText = "v0.2";
        ctx.worldName = "voxhaven.world";
        ctx.seed = world_.seed();
        fillMouseFromRaylib(ctx);
        return ctx;
    }

    void VoxScene::drawUi_() {
        const HudContext ctx = makeHudContext_();

        // Only one menu may draw per frame - they all read the same keys.
        switch (state_) {
        case State::Menu:
            rq().submitUI([this, ctx] { drawTitleScreen(ctx, titleMenu_); });
            break;
        case State::Settings:
            rq().submitUI([this, ctx] { drawSettingsPanel(ctx, settingsUi_); });
            break;
        case State::Paused:
            rq().submitUI([this, ctx] {
                drawHud(ctx);
                drawPauseMenu(ctx, pauseMenu_);
            });
            break;
        case State::Inventory:
            rq().submitUI([this, ctx] {
                drawHud(ctx);
                drawInventoryScreen(ctx, inventory_, inventoryUi_);
            });
            break;
        case State::Playing:
            rq().submitUI([ctx] { drawHud(ctx); });
            break;
        }
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
