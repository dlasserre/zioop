#pragma once

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace gui {
    class Terrain {
    public:
        Terrain() = default;
        Terrain(const Terrain &) = delete;
        Terrain &operator=(const Terrain &) = delete;
        ~Terrain() { clear(); }

        void ensure(std::size_t width, std::size_t height, Texture2D texture);

        [[nodiscard]] float heightAt(float x, float z) const;

        void draw() const;
        void drawTrees() const;

        void clear();

    private:
        struct Tree {
            Vector3 base;
            float scale;
        };

        [[nodiscard]] Mesh buildChunk(int cx0, int cz0, int cellsX, int cellsZ) const;
        [[nodiscard]] float outsideDistance(float x, float z) const;
        void scatterTrees();

        std::size_t m_width{0};
        std::size_t m_height{0};
        Material m_material{};
        std::vector<Mesh> m_chunks;
        std::vector<Tree> m_trees;
    };
}
