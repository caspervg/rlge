#include "ns_game.hpp"

#include <fstream>

#include "runtime.hpp"
#include "transition.hpp"

#include "arena_scene.hpp"
#include "game_over_scene.hpp"
#include "menu_scene.hpp"

namespace neon {
    using namespace rlge;

    namespace {
        constexpr auto kHighScoreFile = "neon_surge.highscore";

        constexpr auto kNebulaShader = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float u_time;
uniform float u_danger;
out vec4 finalColor;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p = p * 2.03 + vec2(17.31, 11.17);
        a *= 0.5;
    }
    return v;
}

void main() {
    vec2 uv = fragTexCoord * vec2(1.6, 1.0);
    float t = u_time * 0.02;
    float warp = fbm(uv * 6.0 - t);
    float n = fbm(uv * 3.0 + vec2(t, -t * 0.7) + warp * 0.55);

    vec3 deep   = vec3(0.030, 0.022, 0.10);
    vec3 violet = vec3(0.16, 0.07, 0.34);
    vec3 pink   = vec3(0.45, 0.10, 0.42);
    vec3 col = mix(deep, violet, smoothstep(0.25, 0.75, n));
    col = mix(col, pink, smoothstep(0.62, 0.95, n) * 0.7);

    vec3 danger = vec3(0.40, 0.05, 0.10);
    col = mix(col, danger, u_danger * smoothstep(0.2, 0.9, n) * 0.8);

    float sparkle = pow(noise(uv * 90.0), 24.0) * (0.6 + 0.4 * sin(u_time * 3.0 + n * 20.0));
    col += vec3(sparkle) * 0.6;

    finalColor = vec4(col, 1.0) * fragColor;
}
)";
    } // namespace

    NsGame::NsGame(Runtime& runtime) :
        runtime_(runtime), assets_(std::make_unique<NsAssets>()) {
        // Dedicated layers shared by every scene: nebula sits below the default
        // background, glow sits between background and world for soft halos.
        nebulaLayer_ = runtime_.layers().create("nebula", -10);
        glowLayer_ = runtime_.layers().create("glow", 45);

        const ShaderHandle nebulaHandle =
            runtime_.assetStore().loadShaderFromMemory("nebula", nullptr, kNebulaShader);
        Shader& nebulaShader = runtime_.assetStore().shader(nebulaHandle);

        ShaderParams<NebulaParams> params(nebulaShader);
        params.bind("u_time", &NebulaParams::time)
              .bind("u_danger", &NebulaParams::danger);
        runtime_.layers().setShaderParams(nebulaLayer_, nebulaHandle, std::move(params));

        loadHighScore_();
        subscribe_();
    }

    NsGame::~NsGame() {
        auto& bus = runtime_.services().gameEvents();
        bus.unsubscribe<StartGameRequested>(startSubId_);
        bus.unsubscribe<RestartRequested>(restartSubId_);
        bus.unsubscribe<BackToMenuRequested>(menuSubId_);
        bus.unsubscribe<GameOverStats>(gameOverSubId_);
    }

    void NsGame::start() {
        runtime_.pushScene<MenuScene>(this);
    }

    void NsGame::finishRun(const GameOverStats& stats) {
        lastRun_ = stats;
        if (stats.score > highScore_) {
            highScore_ = stats.score;
            lastRun_.newHighScore = true;
            saveHighScore_();
        }
        runtime_.transitionTo<GameOverScene>(std::make_unique<FadeTransition>(0.6f, Color{10, 4, 18, 255}), this);
    }

    void NsGame::subscribe_() {
        auto& bus = runtime_.services().gameEvents();

        startSubId_ = bus.subscribe<StartGameRequested>([this](const StartGameRequested&) {
            runtime_.transitionTo<ArenaScene>(std::make_unique<FadeTransition>(0.4f, BLACK), this);
        });
        restartSubId_ = bus.subscribe<RestartRequested>([this](const RestartRequested&) {
            runtime_.transitionTo<ArenaScene>(std::make_unique<FadeTransition>(0.4f, BLACK), this);
        });
        menuSubId_ = bus.subscribe<BackToMenuRequested>([this](const BackToMenuRequested&) {
            runtime_.transitionTo<MenuScene>(std::make_unique<FadeTransition>(0.4f, BLACK), this);
        });
        gameOverSubId_ = bus.subscribe<GameOverStats>([this](const GameOverStats& stats) {
            finishRun(stats);
        });
    }

    void NsGame::loadHighScore_() {
        std::ifstream in(kHighScoreFile);
        if (in) {
            in >> highScore_;
        }
    }

    void NsGame::saveHighScore_() const {
        std::ofstream out(kHighScoreFile, std::ios::trunc);
        if (out) {
            out << highScore_;
        }
    }

} // namespace neon
