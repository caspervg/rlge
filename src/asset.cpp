#include "asset.hpp"
#include "sprite_atlas.hpp"

#include <fstream>
#include <ranges>
#include <sstream>
#include <utility>
#include "imgui.h"

namespace rlge {
    AssetStore::~AssetStore() { unloadAll(); }

    void AssetStore::setRoot(std::filesystem::path root) {
        // Normalize and prefer absolute so joins are predictable.
        if (!root.is_absolute()) {
            root = std::filesystem::absolute(root);
        }
        assetRoot_ = root.lexically_normal();
    }

    std::filesystem::path AssetStore::path(const std::string_view relative) const {
        const std::filesystem::path relPath(relative);
        if (relPath.is_absolute()) {
            return relPath.lexically_normal();
        }
        return (assetRoot_ / relPath).lexically_normal();
    }

    Texture2D& AssetStore::loadTexture(const std::string& id, const std::string& path) {
        const auto it = textures_.find(id);
        if (it != textures_.end())
            return it->second;
        const auto fullPath = this->path(path);
        Texture2D tex = LoadTexture(fullPath.string().c_str());
        auto [iter, _] = textures_.emplace(id, tex);
        return iter->second;
    }

    Texture2D& AssetStore::texture(const std::string& id) { return textures_.at(id); }

    Font& AssetStore::loadFont(const std::string& id, const std::string& path, const int size) {
        const auto it = fonts_.find(id);
        if (it != fonts_.end())
            return it->second;
        Font font = LoadFontEx(this->path(path).string().c_str(), size, nullptr, 0);
        auto [iter, _] = fonts_.emplace(id, font);
        return iter->second;
    }

    Font& AssetStore::font(const std::string& id) { return fonts_.at(id); }

    ShaderHandle AssetStore::loadShader(const std::string& id, const std::string& vertPath,
                                        const std::string& fragPath) {
        if (const auto it = shaderNameToHandle_.find(id); it != shaderNameToHandle_.end()) {
            return it->second;
        }

        const auto v = path(vertPath);
        const auto f = path(fragPath);

        ShaderAsset asset;
        asset.name = id;
        asset.vertPath = v;
        asset.fragPath = f;
        asset.hotReload = hotReload_;

        if (hotReload_) {
            if (std::error_code ec; std::filesystem::exists(v, ec)) {
                asset.lastVertModTime = std::filesystem::last_write_time(v, ec);
            }
            if (std::error_code ec; std::filesystem::exists(f, ec)) {
                asset.lastFragModTime = std::filesystem::last_write_time(f, ec);
            }
        }
        else {
            asset.shader = LoadShader(v.string().c_str(), f.string().c_str());
        }

        const ShaderHandle handle{nextShaderHandle_++};
        auto [iter, _] = shaderAssets_.emplace(handle, std::move(asset));
        shaderNameToHandle_.emplace(id, handle);

        if (hotReload_) {
            reloadShaderAsset_(iter->second);
        }
        return handle;
    }

    ShaderHandle AssetStore::loadVertexShader(const std::string& id, const std::string& vertPath) {
        if (const auto it = shaderNameToHandle_.find(id); it != shaderNameToHandle_.end()) {
            return it->second;
        }

        const auto f = path(vertPath);

        ShaderAsset asset;
        asset.name = id;
        asset.vertPath = f;
        asset.hotReload = hotReload_;

        if (hotReload_) {
            if (std::error_code ec; std::filesystem::exists(f, ec)) {
                asset.lastVertModTime = std::filesystem::last_write_time(f, ec);
            }
        }
        else {
            asset.shader = LoadShader(nullptr, f.string().c_str());
        }

        const ShaderHandle handle{nextShaderHandle_++};
        auto [iter, _] = shaderAssets_.emplace(handle, std::move(asset));
        shaderNameToHandle_.emplace(id, handle);

        if (hotReload_) {
            reloadShaderAsset_(iter->second);
        }
        return handle;
    }

    ShaderHandle AssetStore::loadFragmentShader(const std::string& id, const std::string& fragPath) {
        if (const auto it = shaderNameToHandle_.find(id); it != shaderNameToHandle_.end()) {
            return it->second;
        }

        const auto f = path(fragPath);

        ShaderAsset asset;
        asset.name = id;
        asset.fragPath = f;
        asset.hotReload = hotReload_;

        if (hotReload_) {
            if (std::error_code ec; std::filesystem::exists(f, ec)) {
                asset.lastFragModTime = std::filesystem::last_write_time(f, ec);
            }
        }
        else {
            asset.shader = LoadShader(nullptr, f.string().c_str());
        }

        const ShaderHandle handle{nextShaderHandle_++};
        auto [iter, _] = shaderAssets_.emplace(handle, std::move(asset));
        shaderNameToHandle_.emplace(id, handle);

        if (hotReload_) {
            reloadShaderAsset_(iter->second);
        }
        return handle;
    }

    ShaderHandle AssetStore::loadShaderFromMemory(const std::string& id, const char* vertSrc, const char* fragSrc) {
        if (const auto it = shaderNameToHandle_.find(id); it != shaderNameToHandle_.end()) {
            return it->second;
        }

        ShaderAsset asset;
        asset.name = id;
        asset.shader = LoadShaderFromMemory(vertSrc, fragSrc);
        asset.hotReload = false;

        const ShaderHandle handle{nextShaderHandle_++};
        shaderNameToHandle_.emplace(id, handle);
        shaderAssets_.emplace(handle, std::move(asset));
        return handle;
    }

    Shader& AssetStore::shader(const ShaderHandle handle) { return shaderAssets_.at(handle).shader; }

    ShaderAsset& AssetStore::shaderAsset(const ShaderHandle handle) { return shaderAssets_.at(handle); }

    const ShaderAsset& AssetStore::shaderAsset(const ShaderHandle handle) const { return shaderAssets_.at(handle); }

    void AssetStore::update(const float dt) {
        if (!hotReload_)
            return;

        hotReloadLastCheck_ += dt;
        if (hotReloadLastCheck_ < hotReloadInterval_)
            return;

        hotReloadLastCheck_ = 0.0f;

        for (auto& [handle, asset] : shaderAssets_) {
            if (!asset.hotReload)
                continue;

            bool changed = false;

            if (!asset.vertPath.empty()) {
                std::error_code ec;
                if (std::filesystem::exists(asset.vertPath, ec)) {
                    const auto modTime = std::filesystem::last_write_time(asset.vertPath, ec);
                    if (!ec && modTime != asset.lastVertModTime) {
                        asset.lastVertModTime = modTime;
                        changed = true;
                    }
                }
            }

            if (!asset.fragPath.empty()) {
                std::error_code ec;
                if (std::filesystem::exists(asset.fragPath, ec)) {
                    const auto modTime = std::filesystem::last_write_time(asset.fragPath, ec);
                    if (!ec && modTime != asset.lastFragModTime) {
                        asset.lastFragModTime = modTime;
                        changed = true;
                    }
                }
            }

            if (!changed)
                continue;

            const bool success = reloadShaderAsset_(asset);
            if (shaderReloadCallback_) {
                shaderReloadCallback_(handle, success);
            }
            for (const auto& listener : shaderReloadListeners_) {
                if (listener) {
                    listener(handle, success);
                }
            }
        }
    }

    void AssetStore::hotReload(const bool enable) {
        hotReload_ = enable;
        for (auto& [_, asset] : shaderAssets_) {
            asset.hotReload = enable;
        }
    }

    bool AssetStore::hotReload() const { return hotReload_; }

    void AssetStore::setShaderReloadCallback(ShaderReloadCallback cb) { shaderReloadCallback_ = std::move(cb); }

    void AssetStore::addShaderReloadListener(ShaderReloadCallback cb) {
        shaderReloadListeners_.push_back(std::move(cb));
    }

    void AssetStore::debugOverlay() {
        if (ImGui::Begin("Shaders")) {
            if (shaderAssets_.empty()) {
                ImGui::TextUnformatted("No shaders loaded.");
            }
            else {
                if (ImGui::BeginTable("shaders_table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter)) {
                    ImGui::TableSetupColumn("ID");
                    ImGui::TableSetupColumn("Reloads");
                    ImGui::TableSetupColumn("Status");
                    ImGui::TableSetupColumn("Message");
                    ImGui::TableHeadersRow();

                    for (const auto& [handle, asset] : shaderAssets_) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s (%u)", asset.name.c_str(), handle.value);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%d", asset.reloadCount);

                        ImGui::TableSetColumnIndex(2);
                        if (asset.hasError) {
                            ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, "Error");
                        }
                        else {
                            ImGui::TextColored({0.3f, 1.0f, 0.3f, 1.0f}, "OK");
                        }

                        ImGui::TableSetColumnIndex(3);
                        if (asset.hasError) {
                            ImGui::TextWrapped("%s", asset.errorMessage.c_str());
                        }
                        else {
                            ImGui::TextUnformatted("");
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();
    }

    void AssetStore::unloadAll() {
        for (const auto& val : atlases_ | std::views::values) {
            if (val) {
                val->unload();
            }
        }
        atlases_.clear();

        for (const auto& asset : shaderAssets_ | std::views::values) {
            if (asset.shader.id != 0) {
                UnloadShader(asset.shader);
            }
        }
        shaderAssets_.clear();
        shaderNameToHandle_.clear();

        for (const auto& val : fonts_ | std::views::values) {
            UnloadFont(val);
        }
        fonts_.clear();
        for (const auto& val : textures_ | std::views::values) {
            UnloadTexture(val);
        }
        textures_.clear();
    }

    bool AssetStore::reloadShaderAsset_(ShaderAsset& asset) const {
        asset.hasError = false;
        asset.errorMessage.clear();

        std::string vertSrc, fragSrc;
        const char* vertPtr = nullptr;
        const char* fragPtr = nullptr;

        if (!asset.vertPath.empty()) {
            if (!readFile_(asset.vertPath, vertSrc)) {
                asset.hasError = true;
                asset.errorMessage = "Failed to read vertex shader file: " + asset.vertPath.string();
                return false;
            }
            vertPtr = vertSrc.c_str();
        }

        if (!asset.fragPath.empty()) {
            if (!readFile_(asset.fragPath, fragSrc)) {
                asset.hasError = true;
                asset.errorMessage = "Failed to read fragment shader file: " + asset.fragPath.string();
            }
            fragPtr = fragSrc.c_str();
        }

        const auto newShader = LoadShaderFromMemory(vertPtr, fragPtr);
        if (newShader.id == 0 || !IsShaderValid(newShader)) {
            asset.hasError = true;
            asset.errorMessage =
                "Failed to compile shader: " + asset.vertPath.string() + ", " + asset.fragPath.string();
            return false;
        }

        if (asset.shader.id != 0) {
            UnloadShader(asset.shader);
        }

        asset.shader = newShader;
        asset.reloadCount++;

        return true;
    }

    bool AssetStore::readFile_(const std::filesystem::path& path, std::string& out) const {
        const std::ifstream file(path);
        if (!file)
            return false;
        std::ostringstream ss;
        ss << file.rdbuf();
        out = ss.str();
        return true;
    }
}
