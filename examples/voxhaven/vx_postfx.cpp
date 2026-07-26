#include "vx_postfx.hpp"

#include <algorithm>
#include <cmath>

#include "vx_config.hpp"

namespace vox {

    namespace {
        // Shared vertex stage: raylib's default full-screen blit.
        constexpr auto kVert = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

        // Keeps only what is brighter than the threshold, so lanterns, glowstone
        // and the sun bloom while lit terrain does not wash out.
        constexpr auto kBrightFrag = R"(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float u_threshold;
out vec4 finalColor;
void main() {
    vec3 c = texture(texture0, fragTexCoord).rgb;
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float k = max(lum - u_threshold, 0.0) / max(1.0 - u_threshold, 0.001);
    finalColor = vec4(c * k, 1.0);
}
)";

        // Separable gaussian; run once horizontally, once vertically.
        constexpr auto kBlurFrag = R"(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 u_dir;
uniform vec2 u_resolution;
out vec4 finalColor;
void main() {
    vec2 texel = u_dir / u_resolution;
    float w[5] = float[](0.227027, 0.194594, 0.121621, 0.054054, 0.016216);
    vec3 sum = texture(texture0, fragTexCoord).rgb * w[0];
    for (int i = 1; i < 5; i++) {
        sum += texture(texture0, fragTexCoord + texel * float(i)).rgb * w[i];
        sum += texture(texture0, fragTexCoord - texel * float(i)).rgb * w[i];
    }
    finalColor = vec4(sum, 1.0);
}
)";

        // Final grade: bloom, colour grading, chromatic aberration, vignette,
        // film grain and the underwater ripple all land here in one pass.
        constexpr auto kCompositeFrag = R"(
#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;     // scene
uniform sampler2D u_bloom;
uniform float u_bloomAmount;
uniform float u_time;
uniform vec2 u_resolution;
uniform vec3 u_grade;           // contrast, saturation, brightness
uniform vec4 u_flags;           // vignette, grain, aberration, underwater
out vec4 finalColor;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

void main() {
    vec2 uv = fragTexCoord;

    // Underwater: a slow lateral ripple sells being submerged.
    if (u_flags.w > 0.5) {
        uv.x += sin(uv.y * 34.0 + u_time * 2.4) * 0.0035;
        uv.y += cos(uv.x * 28.0 + u_time * 1.9) * 0.0028;
    }

    vec3 col;
    if (u_flags.z > 0.001) {
        // Chromatic aberration grows toward the edges of the frame.
        vec2 d = (uv - 0.5) * u_flags.z * 0.006;
        col.r = texture(texture0, uv + d).r;
        col.g = texture(texture0, uv).g;
        col.b = texture(texture0, uv - d).b;
    } else {
        col = texture(texture0, uv).rgb;
    }

    if (u_bloomAmount > 0.001) {
        col += texture(u_bloom, uv).rgb * u_bloomAmount;
    }

    // Grade: contrast about mid grey, then saturation, then brightness.
    col = (col - 0.5) * u_grade.x + 0.5;
    float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(vec3(lum), col, u_grade.y);
    col *= u_grade.z;

    if (u_flags.x > 0.001) {
        vec2 v = uv - 0.5;
        float vig = 1.0 - dot(v, v) * u_flags.x * 1.6;
        col *= clamp(vig, 0.0, 1.0);
    }

    if (u_flags.y > 0.001) {
        // Animated grain, scaled by darkness so it reads in shadow, not sky.
        float n = hash(uv * u_resolution + fract(u_time) * 137.0) - 0.5;
        col += n * u_flags.y * 0.09 * (1.0 - lum * 0.7);
    }

    finalColor = vec4(max(col, vec3(0.0)), 1.0);
}
)";
    } // namespace

    void PostFx::init(rlge::AssetStore& assets) {
        bright_ = assets.shader(assets.loadShaderFromMemory("vox_bright", kVert, kBrightFrag));
        blur_ = assets.shader(assets.loadShaderFromMemory("vox_blur", kVert, kBlurFrag));
        composite_ = assets.shader(assets.loadShaderFromMemory("vox_post", kVert, kCompositeFrag));

        locBrightThreshold_ = GetShaderLocation(bright_, "u_threshold");
        locBlurDir_ = GetShaderLocation(blur_, "u_dir");
        locBlurRes_ = GetShaderLocation(blur_, "u_resolution");
        locCompBloomTex_ = GetShaderLocation(composite_, "u_bloom");
        locCompBloom_ = GetShaderLocation(composite_, "u_bloomAmount");
        locCompTime_ = GetShaderLocation(composite_, "u_time");
        locCompRes_ = GetShaderLocation(composite_, "u_resolution");
        locCompGrade_ = GetShaderLocation(composite_, "u_grade");
        locCompFlags_ = GetShaderLocation(composite_, "u_flags");
        ready_ = true;
    }

    void PostFx::releaseTargets_() {
        if (scene_.id != 0) UnloadRenderTexture(scene_);
        if (bloomA_.id != 0) UnloadRenderTexture(bloomA_);
        if (bloomB_.id != 0) UnloadRenderTexture(bloomB_);
        scene_ = RenderTexture2D{};
        bloomA_ = RenderTexture2D{};
        bloomB_ = RenderTexture2D{};
        width_ = 0;
        height_ = 0;
    }

    void PostFx::shutdown() {
        releaseTargets_();
        // Shaders belong to the AssetStore, which unloads them itself.
        ready_ = false;
    }

    bool PostFx::enabled() const { return settings.postFx; }

    void PostFx::ensureTargets_(const int width, const int height) {
        if (width == width_ && height == height_ && scene_.id != 0)
            return;
        releaseTargets_();
        if (width <= 0 || height <= 0)
            return;
        scene_ = LoadRenderTexture(width, height);
        SetTextureFilter(scene_.texture, TEXTURE_FILTER_BILINEAR);
        // Bloom runs at half resolution: it is a blur, so the detail is wasted
        // and the fill cost matters far more.
        const int bw = std::max(1, width / 2);
        const int bh = std::max(1, height / 2);
        bloomA_ = LoadRenderTexture(bw, bh);
        bloomB_ = LoadRenderTexture(bw, bh);
        SetTextureFilter(bloomA_.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(bloomB_.texture, TEXTURE_FILTER_BILINEAR);
        width_ = width;
        height_ = height;
    }

    RenderTexture2D* PostFx::target(const int width, const int height) {
        if (!ready_ || !settings.postFx) {
            if (scene_.id != 0)
                releaseTargets_(); // reclaim VRAM when the chain is switched off
            return nullptr;
        }
        ensureTargets_(width, height);
        return scene_.id != 0 ? &scene_ : nullptr;
    }

    void PostFx::apply(const Rectangle viewport, const float time, const bool underwater,
                       const float dayFactor) {
        if (!ready_ || scene_.id == 0)
            return;

        const auto w = static_cast<float>(scene_.texture.width);
        const auto h = static_cast<float>(scene_.texture.height);
        // Render textures are stored bottom-up, so the source rect is flipped.
        const Rectangle src{0.0f, 0.0f, w, -h};

        const bool wantBloom = settings.bloom && settings.bloomStrength > 0.001f;
        if (wantBloom) {
            const Vector2 bloomRes{static_cast<float>(bloomA_.texture.width),
                                   static_cast<float>(bloomA_.texture.height)};
            const Rectangle bloomDst{0.0f, 0.0f, bloomRes.x, bloomRes.y};
            const Rectangle bloomSrc{0.0f, 0.0f, bloomRes.x, -bloomRes.y};

            // Bright pass into A.
            BeginTextureMode(bloomA_);
            ClearBackground(BLACK);
            BeginShaderMode(bright_);
            SetShaderValue(bright_, locBrightThreshold_, &settings.bloomThreshold,
                           SHADER_UNIFORM_FLOAT);
            DrawTexturePro(scene_.texture, src, bloomDst, {0, 0}, 0.0f, WHITE);
            EndShaderMode();
            EndTextureMode();

            // Blur A -> B horizontally, then B -> A vertically.
            const Vector2 dirH{1.0f, 0.0f};
            const Vector2 dirV{0.0f, 1.0f};
            BeginTextureMode(bloomB_);
            ClearBackground(BLACK);
            BeginShaderMode(blur_);
            SetShaderValue(blur_, locBlurDir_, &dirH, SHADER_UNIFORM_VEC2);
            SetShaderValue(blur_, locBlurRes_, &bloomRes, SHADER_UNIFORM_VEC2);
            DrawTexturePro(bloomA_.texture, bloomSrc, bloomDst, {0, 0}, 0.0f, WHITE);
            EndShaderMode();
            EndTextureMode();

            BeginTextureMode(bloomA_);
            ClearBackground(BLACK);
            BeginShaderMode(blur_);
            SetShaderValue(blur_, locBlurDir_, &dirV, SHADER_UNIFORM_VEC2);
            SetShaderValue(blur_, locBlurRes_, &bloomRes, SHADER_UNIFORM_VEC2);
            DrawTexturePro(bloomB_.texture, bloomSrc, bloomDst, {0, 0}, 0.0f, WHITE);
            EndShaderMode();
            EndTextureMode();
        }

        // Composite to the backbuffer.
        const Vector2 res{w, h};
        const float bloomAmount = wantBloom ? settings.bloomStrength : 0.0f;
        // Night grades cooler and a little flatter, which keeps the moonlit
        // palette from looking like washed-out daylight.
        const float nightMix = 1.0f - std::clamp(dayFactor, 0.0f, 1.0f);
        const Vector3 grade{settings.contrast + nightMix * 0.06f,
                            settings.saturation * (1.0f - nightMix * 0.25f),
                            1.0f};
        const Vector4 flags{settings.postVignette ? settings.vignetteStrength : 0.0f,
                            settings.filmGrain ? settings.grainStrength : 0.0f,
                            settings.aberration ? 1.0f : 0.0f,
                            underwater ? 1.0f : 0.0f};

        BeginShaderMode(composite_);
        SetShaderValue(composite_, locCompBloom_, &bloomAmount, SHADER_UNIFORM_FLOAT);
        SetShaderValue(composite_, locCompTime_, &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(composite_, locCompRes_, &res, SHADER_UNIFORM_VEC2);
        SetShaderValue(composite_, locCompGrade_, &grade, SHADER_UNIFORM_VEC3);
        SetShaderValue(composite_, locCompFlags_, &flags, SHADER_UNIFORM_VEC4);
        if (wantBloom) {
            SetShaderValueTexture(composite_, locCompBloomTex_, bloomA_.texture);
        } else {
            SetShaderValueTexture(composite_, locCompBloomTex_, bloomA_.texture);
        }
        DrawTexturePro(scene_.texture, src, viewport, {0, 0}, 0.0f, WHITE);
        EndShaderMode();
    }

} // namespace vox
