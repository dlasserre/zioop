#include "zioop/gui/Terrain.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace gui {
    namespace {
        constexpr int MARGIN = 12;
        constexpr int CHUNK_CELLS = 16;
        constexpr int RESOLUTION = 4;

        float hash01(const int x, const int z, const unsigned seed) {
            unsigned h = static_cast<unsigned>(x) * 374761393u
                         + static_cast<unsigned>(z) * 668265263u
                         + seed * 2246822519u;
            h = (h ^ (h >> 13)) * 1274126177u;
            h ^= h >> 16;
            return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
        }

        float smooth(const float t) { return t * t * (3.0f - 2.0f * t); }

        float valueNoise(const float x, const float z) {
            const int x0 = static_cast<int>(std::floor(x));
            const int z0 = static_cast<int>(std::floor(z));
            const float tx = smooth(x - static_cast<float>(x0));
            const float tz = smooth(z - static_cast<float>(z0));

            const float a = hash01(x0, z0, 1337);
            const float b = hash01(x0 + 1, z0, 1337);
            const float c = hash01(x0, z0 + 1, 1337);
            const float d = hash01(x0 + 1, z0 + 1, 1337);

            return Lerp(Lerp(a, b, tx), Lerp(c, d, tx), tz);
        }

        float fbm(const float x, const float z) {
            float sum = 0.0f, amplitude = 0.5f, frequency = 1.0f;
            for (int i = 0; i < 3; ++i) {
                sum += amplitude * valueNoise(x * frequency, z * frequency);
                frequency *= 2.0f;
                amplitude *= 0.5f;
            }
            return sum / 0.875f;
        }
    }

    float Terrain::outsideDistance(const float x, const float z) const {
        const float maxX = static_cast<float>(m_width) - 0.5f;
        const float maxZ = static_cast<float>(m_height) - 0.5f;
        const float dx = std::max({-0.5f - x, 0.0f, x - maxX});
        const float dz = std::max({-0.5f - z, 0.0f, z - maxZ});
        return std::hypot(dx, dz);
    }

    float Terrain::heightAt(const float x, const float z) const {
        const float noise = fbm(x * 0.15f, z * 0.15f);
        const float hills = smooth(Clamp(outsideDistance(x, z) / 10.0f, 0.0f, 1.0f));
        const float amplitude = 0.5f + 2.0f * hills;

        return noise * amplitude;
    }

    Mesh Terrain::buildChunk(const int cx0, const int cz0,
                             const int cellsX, const int cellsZ) const {
        const int quadsX = cellsX * RESOLUTION;
        const int quadsZ = cellsZ * RESOLUTION;
        const int vx = quadsX + 1;
        const int vz = quadsZ + 1;

        Mesh mesh{};
        mesh.vertexCount = vx * vz;
        mesh.triangleCount = quadsX * quadsZ * 2;

        mesh.vertices = static_cast<float *>(MemAlloc(mesh.vertexCount * 3 * sizeof(float)));
        mesh.normals = static_cast<float *>(MemAlloc(mesh.vertexCount * 3 * sizeof(float)));
        mesh.texcoords = static_cast<float *>(MemAlloc(mesh.vertexCount * 2 * sizeof(float)));
        mesh.colors = static_cast<unsigned char *>(MemAlloc(mesh.vertexCount * 4));
        mesh.indices = static_cast<unsigned short *>(
            MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short)));

        const float originX = static_cast<float>(cx0) - 0.5f;
        const float originZ = static_cast<float>(cz0) - 0.5f;
        constexpr float step = 1.0f / RESOLUTION;
        const Vector3 sun = Vector3Normalize({-0.5f, 0.8f, -0.3f});

        for (int j = 0; j < vz; ++j) {
            for (int i = 0; i < vx; ++i) {
                const int idx = j * vx + i;
                const float x = originX + static_cast<float>(i) * step;
                const float z = originZ + static_cast<float>(j) * step;
                const float y = heightAt(x, z);

                mesh.vertices[idx * 3 + 0] = x;
                mesh.vertices[idx * 3 + 1] = y;
                mesh.vertices[idx * 3 + 2] = z;

                constexpr float e = 0.1f;
                const float dhdx = (heightAt(x + e, z) - heightAt(x - e, z)) / (2.0f * e);
                const float dhdz = (heightAt(x, z + e) - heightAt(x, z - e)) / (2.0f * e);
                const Vector3 n = Vector3Normalize({-dhdx, 1.0f, -dhdz});

                mesh.normals[idx * 3 + 0] = n.x;
                mesh.normals[idx * 3 + 1] = n.y;
                mesh.normals[idx * 3 + 2] = n.z;

                mesh.texcoords[idx * 2 + 0] = x + 0.5f;
                mesh.texcoords[idx * 2 + 1] = z + 0.5f;

                const float lambert = std::max(0.0f, Vector3DotProduct(n, sun));
                const float light = 0.45f + 0.55f * lambert;
                const float rock = Clamp((y - 0.6f) / 1.2f, 0.0f, 1.0f);

                mesh.colors[idx * 4 + 0] = static_cast<unsigned char>(255.0f * light * Lerp(1.0f, 0.75f, rock));
                mesh.colors[idx * 4 + 1] = static_cast<unsigned char>(255.0f * light * Lerp(1.0f, 0.70f, rock));
                mesh.colors[idx * 4 + 2] = static_cast<unsigned char>(255.0f * light * Lerp(1.0f, 0.60f, rock));
                mesh.colors[idx * 4 + 3] = 255;
            }
        }

        int t = 0;
        for (int j = 0; j < quadsZ; ++j) {
            for (int i = 0; i < quadsX; ++i) {
                const auto a = static_cast<unsigned short>(j * vx + i);
                const auto b = static_cast<unsigned short>(a + 1);
                const auto c = static_cast<unsigned short>(a + vx);
                const auto d = static_cast<unsigned short>(c + 1);

                mesh.indices[t++] = a; mesh.indices[t++] = c; mesh.indices[t++] = b;
                mesh.indices[t++] = b; mesh.indices[t++] = c; mesh.indices[t++] = d;
            }
        }

        UploadMesh(&mesh, false);
        return mesh;
    }

    void Terrain::ensure(const std::size_t width, const std::size_t height,
                         const Texture2D texture) {
        if (width == 0 || height == 0) {
            return;
        }
        if (width == m_width && height == m_height && !m_chunks.empty()) {
            return;
        }

        clear();
        m_width = width;
        m_height = height;

        m_material = LoadMaterialDefault();
        SetMaterialTexture(&m_material, MATERIAL_MAP_DIFFUSE, texture);

        const int endX = static_cast<int>(width) + MARGIN;
        const int endZ = static_cast<int>(height) + MARGIN;

        for (int cz = -MARGIN; cz < endZ; cz += CHUNK_CELLS) {
            for (int cx = -MARGIN; cx < endX; cx += CHUNK_CELLS) {
                m_chunks.push_back(buildChunk(
                    cx, cz,
                    std::min(CHUNK_CELLS, endX - cx),
                    std::min(CHUNK_CELLS, endZ - cz)));
            }
        }

        scatterTrees();
    }

    void Terrain::scatterTrees() {
        m_trees.clear();

        const int w = static_cast<int>(m_width);
        const int h = static_cast<int>(m_height);

        for (int z = -MARGIN; z < h + MARGIN; ++z) {
            for (int x = -MARGIN; x < w + MARGIN; ++x) {
                const bool inside = x >= 0 && x < w && z >= 0 && z < h;
                const float roll = hash01(x, z, 7);
                Vector3 pos{};

                if (inside) {
                    if (roll > 0.04f) continue;
                    pos.x = static_cast<float>(x) + 0.5f;
                    pos.z = static_cast<float>(z) + 0.5f;
                } else {
                    if (roll > 0.25f) continue;
                    pos.x = static_cast<float>(x) + (hash01(x, z, 11) - 0.5f) * 0.9f;
                    pos.z = static_cast<float>(z) + (hash01(x, z, 13) - 0.5f) * 0.9f;
                }

                pos.y = heightAt(pos.x, pos.z);
                const float scale = inside ? 0.7f : 1.0f + hash01(x, z, 17) * 0.8f;
                m_trees.push_back({pos, scale});
            }
        }
    }

    void Terrain::draw() const {
        for (const Mesh &chunk: m_chunks) {
            DrawMesh(chunk, m_material, MatrixIdentity());
        }
    }

    void Terrain::drawTrees() const {
        for (const auto &[b, s]: m_trees) {
            DrawCylinder(b, 0.05f * s, 0.07f * s, 0.30f * s, 6, BROWN);
            DrawCylinder({b.x, b.y + 0.25f * s, b.z}, 0.0f, 0.32f * s, 0.55f * s, 7, DARKGREEN);
            DrawCylinder({b.x, b.y + 0.50f * s, b.z}, 0.0f, 0.24f * s, 0.45f * s, 7, GREEN);
        }
    }

    void Terrain::clear() {
        for (const Mesh &chunk: m_chunks) {
            UnloadMesh(chunk);
        }
        m_chunks.clear();
        m_trees.clear();
        m_width = m_height = 0;
        if (m_material.maps != nullptr) {
            MemFree(m_material.maps);
            m_material = {};
        }
    }
}
