#include "zioop/gui/Assets.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace gui {
    namespace {
        unsigned char toSrgb(const unsigned char linear) {
            const float value = std::pow(static_cast<float>(linear) / 255.0f, 1.0f / 2.2f);
            return static_cast<unsigned char>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        }

        nlohmann::json readGltfDocument(const std::string &path) {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return {};
            }
            const std::vector<char> bytes{std::istreambuf_iterator<char>(file), {}};

            if (path.ends_with(".glb")) {
                if (bytes.size() < 20) {
                    return {};
                }
                std::uint32_t length{};
                std::memcpy(&length, bytes.data() + 12, sizeof(length));
                if (20 + static_cast<std::size_t>(length) > bytes.size()) {
                    return {};
                }
                return nlohmann::json::parse(bytes.begin() + 20, bytes.begin() + 20 + length, nullptr, false);
            }

            return nlohmann::json::parse(bytes.begin(), bytes.end(), nullptr, false);
        }
    }

    std::string assetPath(const std::string_view relative) {
        return (std::filesystem::path(GetApplicationDirectory()) / "assets" / relative).string();
    }

    void correctMaterialColors(Model &model) {
        for (int i = 0; i < model.materialCount; ++i) {
            Color &color = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color;
            color = {toSrgb(color.r), toSrgb(color.g), toSrgb(color.b), color.a};
        }
    }

    void unloadModel(LoadedModel &loaded) {
        if (loaded.animations != nullptr) {
            UnloadModelAnimations(loaded.animations, loaded.animationCount);
            loaded.animations = nullptr;
            loaded.animationCount = 0;
        }
        UnloadModel(loaded.model);
        loaded.model = {};
    }

    LoadedModel loadModel(
        const std::string_view relative,
        const std::initializer_list<std::string_view> emissiveMaterials
    ) {
        const auto path = assetPath(relative);
        if (!FileExists(path.c_str())) {
            throw std::runtime_error("Missing model " + path);
        }

        LoadedModel loaded;
        loaded.model = LoadModel(path.c_str());
        if (!IsModelValid(loaded.model)) {
            throw std::runtime_error("Unable to load model " + path);
        }

        correctMaterialColors(loaded.model);
        loaded.bounds = GetModelBoundingBox(loaded.model);

        if (loaded.model.boneCount > 0) {
            loaded.animations = LoadModelAnimations(path.c_str(), &loaded.animationCount);
            if (loaded.animationCount == 0 || !IsModelAnimationValid(loaded.model, loaded.animations[0])) {
                UnloadModelAnimations(loaded.animations, loaded.animationCount);
                loaded.animations = nullptr;
                loaded.animationCount = 0;
            }
        }
        loaded.emissive.assign(static_cast<std::size_t>(loaded.model.materialCount), 0.0f);

        if (emissiveMaterials.size() > 0) {
            const auto document = readGltfDocument(path);
            if (document.is_object() && document.contains("materials")) {
                const auto &materials = document.at("materials");
                for (std::size_t i = 0; i < materials.size() && i + 1 < loaded.emissive.size(); ++i) {
                    const auto name = materials[i].value("name", std::string{});
                    if (std::ranges::find(emissiveMaterials, name) == emissiveMaterials.end()) {
                        continue;
                    }
                    loaded.emissive[i + 1] = 1.0f;
                    Color &color = loaded.model.materials[i + 1].maps[MATERIAL_MAP_DIFFUSE].color;
                    if (std::min({color.r, color.g, color.b}) > 180) {
                        color = {255, 150, 50, color.a};
                    }
                }
            }
        }

        return loaded;
    }
}
