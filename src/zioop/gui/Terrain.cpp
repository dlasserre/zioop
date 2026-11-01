#include "zioop/gui/Terrain.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace gui {
    float hash01(const int x, const int z, const unsigned seed) {
        unsigned h = static_cast<unsigned>(x) * 374761393u
                     + static_cast<unsigned>(z) * 668265263u
                     + seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
    }

    namespace {
        constexpr int MARGIN = 12;
        constexpr int CHUNK_CELLS = 16;
        constexpr int RESOLUTION = 4;
        constexpr float SHADOW_REACH = 7.0f;

        constexpr Vector2 CELL_SPOTS[8] = {
            {-0.5f, -0.5f}, {0.5f, -0.5f}, {-0.5f, 0.5f}, {0.5f, 0.5f},
            {0.0f, -0.5f}, {0.0f, 0.5f}, {-0.5f, 0.0f}, {0.5f, 0.0f}
        };

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

        float clump(const int x, const int z, const float frequency, const float offset) {
            return smooth(Clamp(
                (fbm(static_cast<float>(x) * frequency + offset,
                     static_cast<float>(z) * frequency + offset) - 0.5f) / 0.25f,
                0.0f, 1.0f));
        }

        float flicker(const float time, const float seed) {
            return 0.8f + 0.12f * std::sin(time * 9.0f + seed * 1.7f) + 0.08f * std::sin(time * 23.0f + seed * 3.1f);
        }
    }

    void Terrain::loadProps() {
        const auto add = [this](const Kind kind, const std::string &name,
                                const std::initializer_list<std::string_view> emissive = {}) {
            m_props.push_back(loadModel("nature/" + name + ".glb", emissive));
            m_kinds[static_cast<std::size_t>(kind)].push_back(m_props.size() - 1);
        };

        for (int i = 1; i <= 5; ++i) {
            add(Kind::Tree, "CommonTree_" + std::to_string(i));
            add(Kind::Pine, "PineTree_" + std::to_string(i));
        }
        for (int i = 1; i <= 3; ++i) {
            add(Kind::Tree, "BirchTree_" + std::to_string(i));
        }
        for (const char *name: {"Bush_1", "Bush_2", "BushBerries_1"}) {
            add(Kind::Bush, name);
        }
        for (int i = 1; i <= 7; ++i) {
            add(Kind::Rock, "Rock_" + std::to_string(i));
        }
        for (int i = 1; i <= 3; ++i) {
            add(Kind::Rock, "Rock_Moss_" + std::to_string(i));
        }
        for (const char *name: {"Grass", "Grass_2", "Grass_Short"}) {
            add(Kind::Grass, name);
        }
        for (const char *name: {"Flowers", "Plant_1", "Plant_2", "Plant_3"}) {
            add(Kind::Flower, name);
        }
        for (const char *name: {"TreeStump", "TreeStump_Moss", "WoodLog_Moss"}) {
            add(Kind::Deco, name);
        }
        add(Kind::Torch, "Torch", {"Fire"});
        add(Kind::Bonfire, "Bonfire_Lit", {"Fire"});
        add(Kind::Fence, "Fence");

        Image glow = GenImageGradientRadial(64, 64, 0.0f, WHITE, BLANK);
        m_glow = LoadTextureFromImage(glow);
        UnloadImage(glow);
        SetTextureFilter(m_glow, TEXTURE_FILTER_BILINEAR);
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

                const float variation = 0.88f + 0.2f * fbm(x * 0.6f + 31.0f, z * 0.6f + 31.0f);
                const float rock = Clamp((y - 0.6f) / 1.2f, 0.0f, 1.0f);
                const float steep = Clamp((1.0f - n.y) * 4.0f, 0.0f, 1.0f);
                const float bare = std::max(rock, steep * 0.6f);
                const float meadow = smooth(Clamp((outsideDistance(x, z) - 0.2f) / 1.5f, 0.0f, 1.0f))
                                     * (1.0f - bare)
                                     * (0.75f + 0.25f * fbm(x * 0.4f + 77.0f, z * 0.4f + 77.0f));

                const Vector3 dirt{235.0f, 240.0f, 220.0f};
                const Vector3 grass{150.0f, 215.0f, 95.0f};
                const Vector3 tint = Vector3Scale(Vector3Lerp(dirt, grass, meadow), variation);

                mesh.colors[idx * 4 + 0] = static_cast<unsigned char>(Clamp(tint.x * Lerp(1.0f, 0.85f, bare), 0.0f, 255.0f));
                mesh.colors[idx * 4 + 1] = static_cast<unsigned char>(Clamp(tint.y * Lerp(1.0f, 0.75f, bare), 0.0f, 255.0f));
                mesh.colors[idx * 4 + 2] = static_cast<unsigned char>(Clamp(tint.z * Lerp(1.0f, 0.70f, bare), 0.0f, 255.0f));
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

        clearChunks();
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

        scatter();
    }


    bool Terrain::blocked(const Vector2 position, const float radius) const {
        for (const auto &[from, to, extent]: m_footprints) {
            const Vector2 segment = Vector2Subtract(to, from);
            const float lengthSqr = Vector2LengthSqr(segment);
            const float t = lengthSqr > 0.0f
                                ? Clamp(Vector2DotProduct(Vector2Subtract(position, from), segment) / lengthSqr, 0.0f, 1.0f)
                                : 0.0f;
            const Vector2 closest = Vector2Add(from, Vector2Scale(segment, t));
            if (Vector2Distance(position, closest) < radius + extent) {
                return true;
            }
        }
        return false;
    }

    bool Terrain::place(
        const Kind kind,
        const float roll,
        const Vector3 position,
        const float height,
        const float angle
    ) {
        const auto &variants = m_kinds[static_cast<std::size_t>(kind)];
        if (variants.empty()) {
            return false;
        }

        const auto index = variants[static_cast<std::size_t>(roll * static_cast<float>(variants.size())) % variants.size()];
        const auto &bounds = m_props[index].bounds;
        const float modelHeight = std::max(bounds.max.y - bounds.min.y, 0.0001f);
        return placeScaled(kind, index, position, height / modelHeight, angle);
    }

    bool Terrain::placeScaled(
        const Kind kind,
        const std::size_t index,
        const Vector3 position,
        const float scale,
        const float angle
    ) {
        const auto &bounds = m_props[index].bounds;
        const float halfWidth = std::max(bounds.max.x - bounds.min.x, bounds.max.z - bounds.min.z) * 0.5f * scale;

        float footprint = 0.0f;
        bool solid = true;
        bool castsShadow = true;
        switch (kind) {
            case Kind::Tree:
            case Kind::Pine: footprint = halfWidth * 0.3f; break;
            case Kind::Rock: footprint = halfWidth * 0.85f; break;
            case Kind::Bush: footprint = halfWidth * 0.8f; break;
            case Kind::Deco: footprint = halfWidth * 0.7f; break;
            case Kind::Torch: footprint = std::max(halfWidth, 0.08f); break;
            case Kind::Bonfire: footprint = halfWidth; break;
            case Kind::Fence: solid = false; break;
            case Kind::Grass:
            case Kind::Flower:
                footprint = halfWidth * 0.4f;
                solid = false;
                castsShadow = false;
                break;
            case Kind::Count: break;
        }

        const Vector2 center{position.x, position.z};
        const bool structure = kind == Kind::Torch || kind == Kind::Bonfire || kind == Kind::Fence;
        if (!structure && blocked(center, footprint)) {
            return false;
        }
        if (solid) {
            m_footprints.push_back({center, center, footprint});
        }

        const float ground = heightAt(position.x, position.z);
        const Matrix transform = MatrixMultiply(
            MatrixMultiply(MatrixScale(scale, scale, scale), MatrixRotateY(angle)),
            MatrixTranslate(position.x, ground - bounds.min.y * scale, position.z)
        );

        m_instances.push_back({
            .prop = index,
            .transform = transform,
            .castsShadow = castsShadow && outsideDistance(position.x, position.z) < SHADOW_REACH
        });
        return true;
    }

    void Terrain::surroundWithFence() {
        const auto &variants = m_kinds[static_cast<std::size_t>(Kind::Fence)];
        if (variants.empty()) {
            return;
        }

        constexpr float gap = 0.2f;
        constexpr float targetLength = 0.5f;
        const auto index = variants.front();
        const auto &bounds = m_props[index].bounds;
        const float modelLength = std::max(bounds.max.x - bounds.min.x, 0.0001f);
        const float modelCenter = (bounds.max.x + bounds.min.x) * 0.5f;

        const float minX = -0.5f - gap;
        const float minZ = -0.5f - gap;
        const float maxX = static_cast<float>(m_width) - 0.5f + gap;
        const float maxZ = static_cast<float>(m_height) - 0.5f + gap;

        const auto side = [&](const Vector3 from, const Vector3 to, const float angle) {
            const float length = Vector3Distance(from, to);
            const int count = std::max(1, static_cast<int>(std::ceil(length / targetLength)));
            const float segment = length / static_cast<float>(count);
            const float scale = segment / modelLength;
            const Vector3 direction = Vector3Normalize(Vector3Subtract(to, from));

            for (int i = 0; i < count; ++i) {
                const float along = (static_cast<float>(i) + 0.5f) * segment - modelCenter * scale;
                placeScaled(Kind::Fence, index, Vector3Add(from, Vector3Scale(direction, along)), scale, angle);
            }
            m_footprints.push_back({{from.x, from.z}, {to.x, to.z}, 0.06f});
        };

        side({minX, 0.0f, minZ}, {maxX, 0.0f, minZ}, 0.0f);
        side({maxX, 0.0f, maxZ}, {minX, 0.0f, maxZ}, PI);
        side({minX, 0.0f, maxZ}, {minX, 0.0f, minZ}, PI / 2.0f);
        side({maxX, 0.0f, minZ}, {maxX, 0.0f, maxZ}, -PI / 2.0f);
    }

    void Terrain::addFlame(const Kind kind, const Vector3 position, const float height, const float angle) {
        if (!place(kind, 0.0f, position, height, angle)) {
            return;
        }

        const bool torch = kind == Kind::Torch;
        const auto flame = static_cast<int>(m_flames.size());
        m_instances.back().flame = flame;
        m_flames.push_back({
            .position = {
                position.x,
                heightAt(position.x, position.z) + (torch ? height * 0.9f : height * 0.45f),
                position.z
            },
            .size = torch ? 0.32f : 0.7f,
            .lightRadius = torch ? 3.2f : 6.0f,
            .lightStrength = torch ? 1.0f : 1.6f
        });
    }

    void Terrain::scatter() {
        m_instances.clear();
        m_footprints.clear();
        m_flames.clear();

        surroundWithFence();

        const int w = static_cast<int>(m_width);
        const int h = static_cast<int>(m_height);

        addFlame(Kind::Bonfire, {-2.4f, 0.0f, -2.4f}, 0.45f, 0.6f);
        m_footprints.back().radius += 0.5f;

        constexpr int torchSpacing = 3;
        constexpr float torchHeight = 0.6f;
        constexpr float torchOffset = 0.95f;
        const auto addTorch = [&](const float x, const float z) {
            const float angle = hash01(static_cast<int>(x * 10.0f), static_cast<int>(z * 10.0f), 401) * 2.0f * PI;
            addFlame(Kind::Torch, {x, 0.0f, z}, torchHeight, angle);
        };
        for (int x = 0; x < w; x += torchSpacing) {
            addTorch(static_cast<float>(x), -torchOffset);
            addTorch(static_cast<float>(x), static_cast<float>(h - 1) + torchOffset);
        }
        for (int z = 0; z < h; z += torchSpacing) {
            addTorch(-torchOffset, static_cast<float>(z));
            addTorch(static_cast<float>(w - 1) + torchOffset, static_cast<float>(z));
        }

        scatterSolids();
        scatterGround();
    }

    void Terrain::scatterSolids() {
        const int w = static_cast<int>(m_width);
        const int h = static_cast<int>(m_height);

        for (int z = -MARGIN; z < h + MARGIN; ++z) {
            for (int x = -MARGIN; x < w + MARGIN; ++x) {
                const bool inside = x >= 0 && x < w && z >= 0 && z < h;
                const float fx = static_cast<float>(x);
                const float fz = static_cast<float>(z);
                const float distance = outsideDistance(fx, fz);
                const float angle = hash01(x, z, 3) * 2.0f * PI;

                if (inside) {
                    if (hash01(x, z, 53) < 0.03f + 0.05f * clump(x, z, 0.25f, 120.0f)) {
                        const Vector2 spot = CELL_SPOTS[static_cast<int>(hash01(x, z, 59) * 8.0f) % 8];
                        place(Kind::Rock, hash01(x, z, 61), {fx + spot.x, 0.0f, fz + spot.y},
                              0.04f + 0.04f * hash01(x, z, 71), angle);
                    }

                    if (hash01(x, z, 101) < 0.03f + 0.06f * clump(x, z, 0.3f, 200.0f)) {
                        const Vector2 spot = CELL_SPOTS[static_cast<int>(hash01(x, z, 103) * 8.0f) % 8];
                        place(Kind::Bush, hash01(x, z, 107), {fx + spot.x, 0.0f, fz + spot.y},
                              0.12f + 0.08f * hash01(x, z, 113), angle);
                    }
                    continue;
                }

                const Vector3 jitter{
                    fx + (hash01(x, z, 11) - 0.5f) * 0.9f,
                    0.0f,
                    fz + (hash01(x, z, 13) - 0.5f) * 0.9f
                };
                if (outsideDistance(jitter.x, jitter.z) < 0.8f) {
                    continue;
                }

                const float roll = hash01(x, z, 7);
                if (distance > 1.6f && roll < 0.36f) {
                    const bool pine = hash01(x, z, 37) < 0.35f + 0.3f * smooth(Clamp(distance / 10.0f, 0.0f, 1.0f));
                    place(pine ? Kind::Pine : Kind::Tree, hash01(x, z, 29), jitter,
                          1.3f + 1.5f * hash01(x, z, 17), angle);
                } else if (roll < 0.43f) {
                    place(Kind::Rock, hash01(x, z, 61), jitter, 0.2f + 0.6f * hash01(x, z, 71), angle);
                } else if (roll < 0.55f) {
                    place(Kind::Bush, hash01(x, z, 107), jitter, 0.3f + 0.3f * hash01(x, z, 113), angle);
                } else if (roll < 0.58f && distance > 1.6f) {
                    place(Kind::Deco, hash01(x, z, 131), jitter, 0.15f + 0.15f * hash01(x, z, 137), angle);
                }
            }
        }
    }

    void Terrain::scatterGround() {
        const int w = static_cast<int>(m_width);
        const int h = static_cast<int>(m_height);

        for (int z = -MARGIN; z < h + MARGIN; ++z) {
            for (int x = -MARGIN; x < w + MARGIN; ++x) {
                const bool inside = x >= 0 && x < w && z >= 0 && z < h;
                const float fx = static_cast<float>(x);
                const float fz = static_cast<float>(z);
                const int attempts = inside ? 4 : 2;
                const float density = inside ? 0.7f : 0.5f;

                for (int k = 0; k < attempts; ++k) {
                    const auto seed = static_cast<unsigned>((inside ? 500 : 700) + k * 10);
                    if (hash01(x, z, seed) > density) {
                        continue;
                    }
                    const Vector3 position{
                        fx + (hash01(x, z, seed + 1) - 0.5f) * 0.95f,
                        0.0f,
                        fz + (hash01(x, z, seed + 2) - 0.5f) * 0.95f
                    };
                    const bool flower = hash01(x, z, seed + 3) < (inside ? 0.18f : 0.2f);
                    const float size = inside
                                           ? (flower ? 0.09f + 0.05f * hash01(x, z, seed + 5) : 0.07f + 0.06f * hash01(x, z, seed + 5))
                                           : 0.14f + 0.12f * hash01(x, z, seed + 5);
                    place(flower ? Kind::Flower : Kind::Grass, hash01(x, z, seed + 4), position, size,
                          hash01(x, z, seed + 6) * 2.0f * PI);
                }
            }
        }
    }

    void Terrain::drawInstance(const Lighting &lighting, const Instance &instance, const float time) const {
        const auto &prop = m_props[instance.prop];

        if (instance.flame < 0) {
            lighting.drawModel(prop.model, instance.transform, WHITE, prop.emissive);
            return;
        }

        const float seed = static_cast<float>(instance.flame);
        if (prop.animationCount > 0) {
            const ModelAnimation &animation = prop.animations[0];
            const int frame = static_cast<int>((time + seed * 0.37f) * 30.0f) % std::max(animation.frameCount, 1);
            UpdateModelAnimation(prop.model, animation, frame);
        }
        lighting.drawModel(prop.model, instance.transform, WHITE, prop.emissive, 0.7f + 0.6f * flicker(time, seed));
    }

    void Terrain::draw(const Lighting &lighting, const float time) const {
        for (const Mesh &chunk: m_chunks) {
            lighting.drawMesh(chunk, m_material, MatrixIdentity());
        }
        for (const auto &instance: m_instances) {
            drawInstance(lighting, instance, time);
        }
    }

    void Terrain::drawShadowCasters(const Lighting &lighting, const float time) const {
        for (const auto &instance: m_instances) {
            if (instance.castsShadow) {
                drawInstance(lighting, instance, time);
            }
        }
    }

    void Terrain::drawGlow(const Camera3D &camera, const float time, const float night) const {
        if (m_glow.id == 0 || m_flames.empty()) {
            return;
        }

        const float strength = 0.35f + 0.65f * night;

        rlDrawRenderBatchActive();
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthMask();

        for (std::size_t i = 0; i < m_flames.size(); ++i) {
            const auto &flame = m_flames[i];
            const float seed = static_cast<float>(i);
            const float pulse = flicker(time, seed);

            DrawBillboard(camera, m_glow, flame.position, flame.size * (3.0f + 0.4f * pulse),
                          Fade(Color{255, 120, 40, 255}, 0.45f * strength * pulse));
            DrawBillboard(camera, m_glow, flame.position, flame.size * (0.9f + 0.2f * pulse),
                          Fade(Color{255, 200, 120, 255}, 0.75f * strength * pulse));

            for (int k = 0; k < 6; ++k) {
                const float offset = hash01(static_cast<int>(i), k, 811);
                const float life = std::fmod(time * (0.45f + 0.3f * offset) + offset, 1.0f);
                const float sway = std::sin(time * 3.0f + offset * 20.0f) * 0.12f * flame.size;
                const Vector3 ember{
                    flame.position.x + sway + (offset - 0.5f) * 0.3f * flame.size,
                    flame.position.y + life * flame.size * 2.2f,
                    flame.position.z + std::cos(time * 2.3f + offset * 13.0f) * 0.12f * flame.size
                };
                DrawBillboard(camera, m_glow, ember, flame.size * 0.12f,
                              Fade(Color{255, 170, 70, 255}, (1.0f - life) * (0.5f + 0.5f * strength)));
            }
        }

        EndBlendMode();
        rlEnableDepthMask();
    }

    void Terrain::collectLights(std::vector<PointLight> &lights, const float time, const float night) const {
        const float base = 0.4f + 2.2f * night;
        for (std::size_t i = 0; i < m_flames.size(); ++i) {
            const auto &flame = m_flames[i];
            const float intensity = base * flame.lightStrength * flicker(time, static_cast<float>(i));
            lights.push_back({
                .position = flame.position,
                .color = Vector3Scale({1.0f, 0.48f, 0.16f}, intensity),
                .radius = flame.lightRadius
            });
        }
    }

    void Terrain::clearChunks() {
        for (const Mesh &chunk: m_chunks) {
            UnloadMesh(chunk);
        }
        m_chunks.clear();
        m_instances.clear();
        m_footprints.clear();
        m_flames.clear();
        m_width = m_height = 0;
        if (m_material.maps != nullptr) {
            MemFree(m_material.maps);
            m_material = {};
        }
    }

    void Terrain::unload() {
        clearChunks();
        for (auto &prop: m_props) {
            unloadModel(prop);
        }
        m_props.clear();
        for (auto &kind: m_kinds) {
            kind.clear();
        }
        if (m_glow.id != 0) {
            UnloadTexture(m_glow);
            m_glow = {};
        }
    }
}
