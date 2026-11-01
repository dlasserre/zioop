#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>
#include <vector>

#include "zioop/gui/Assets.hpp"
#include "zioop/gui/Lighting.hpp"

namespace gui {
    [[nodiscard]] float hash01(int x, int z, unsigned seed);

    class Terrain {
    public:
        Terrain() = default;
        Terrain(const Terrain &) = delete;
        Terrain &operator=(const Terrain &) = delete;
        ~Terrain() = default;

        void loadProps();

        void ensure(std::size_t width, std::size_t height, Texture2D texture);

        [[nodiscard]] float heightAt(float x, float z) const;

        void draw(const Lighting &lighting, float time) const;

        void drawShadowCasters(const Lighting &lighting, float time) const;

        void drawGlow(const Camera3D &camera, float time, float night) const;

        void collectLights(std::vector<PointLight> &lights, float time, float night) const;

        void unload();

    private:
        enum class Kind : std::size_t { Tree, Pine, Bush, Rock, Grass, Flower, Deco, Torch, Bonfire, Fence, Count };

        struct Instance {
            std::size_t prop;
            Matrix transform;
            bool castsShadow;
            int flame{-1};
        };

        struct Flame {
            Vector3 position;
            float size;
            float lightRadius;
            float lightStrength;
        };

        struct Footprint {
            Vector2 from;
            Vector2 to;
            float radius;
        };

        void scatter();

        void scatterSolids();

        void scatterGround();

        bool place(Kind kind, float roll, Vector3 position, float height, float angle);

        bool placeScaled(Kind kind, std::size_t prop, Vector3 position, float scale, float angle);

        void surroundWithFence();

        void addFlame(Kind kind, Vector3 position, float height, float angle);

        [[nodiscard]] bool blocked(Vector2 position, float radius) const;

        void drawInstance(const Lighting &lighting, const Instance &instance, float time) const;

        [[nodiscard]] Mesh buildChunk(int cx0, int cz0, int cellsX, int cellsZ) const;
        [[nodiscard]] float outsideDistance(float x, float z) const;

        void clearChunks();

        std::size_t m_width{0};
        std::size_t m_height{0};
        Material m_material{};
        std::vector<Mesh> m_chunks;

        std::vector<LoadedModel> m_props;
        std::array<std::vector<std::size_t>, static_cast<std::size_t>(Kind::Count)> m_kinds{};
        std::vector<Instance> m_instances;
        std::vector<Footprint> m_footprints;
        std::vector<Flame> m_flames;
        Texture2D m_glow{};
    };
}
