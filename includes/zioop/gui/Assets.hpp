#pragma once

#include <raylib.h>

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace gui {
    struct LoadedModel {
        Model model{};
        std::vector<float> emissive;
        BoundingBox bounds{};
        ModelAnimation *animations{nullptr};
        int animationCount{0};
    };

    void unloadModel(LoadedModel &loaded);

    [[nodiscard]] std::string assetPath(std::string_view relative);

    void correctMaterialColors(Model &model);

    [[nodiscard]] LoadedModel loadModel(
        std::string_view relative,
        std::initializer_list<std::string_view> emissiveMaterials = {}
    );
}
