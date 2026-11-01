#include "zioop/gui/Renderer.hpp"
#include "zioop/gui/Renderer.hpp"

#include <cctype>
#include <chrono>
#include <limits>
#include <format>
#include <iostream>
#include <string>
#include <algorithm>
#include <set>
#include <raymath.h>
#include <rlgl.h>
#include <rcamera.h>

#include "zioop/core/Resource.hpp"
#include "zioop/core/Types.hpp"
#include "zioop/gui/Assets.hpp"

extern "C" const char *glfwGetKeyName(int key, int scancode);

namespace gui {
    using Clock = std::chrono::steady_clock;

    constexpr float LookDuration = 1.6f;

    static Vector3 LookVector(const int direction) {
        switch (static_cast<core::Direction>(direction)) {
            case core::Direction::NORTH: return {0.0f, 0.0f, -1.0f};
            case core::Direction::SOUTH: return {0.0f, 0.0f, 1.0f};
            case core::Direction::EAST: return {1.0f, 0.0f, 0.0f};
            case core::Direction::WEST: return {-1.0f, 0.0f, 0.0f};
        }
        return {0.0f, 0.0f, -1.0f};
    }

    static float LookLength(const std::size_t level) {
        return std::max(0.8f, static_cast<float>(level) - 0.4f) * zioop::CELL_SIZE;
    }

    static float LookFade(const float time) {
        if (time < 1.1f) {
            return 1.0f;
        }
        return std::clamp(1.0f - (time - 1.1f) / (LookDuration - 1.1f), 0.0f, 1.0f);
    }

    static Color AgentColor(const std::size_t id) {
        return ColorFromHSV(static_cast<float>((id % 12) * 30), 0.7f, 0.8f);
    }

    namespace ui {
        constexpr Color Surface{16, 20, 28, 222};
        constexpr Color SurfaceRaised{30, 36, 48, 235};
        constexpr Color Outline{255, 255, 255, 30};
        constexpr Color TextPrimary{240, 243, 248, 255};
        constexpr Color TextSecondary{160, 170, 186, 255};
        constexpr Color Gold{255, 204, 102, 255};
    }

    static void DrawPanel(const Rectangle rectangle, const float radius, const Color fill,
                          const Color outline = ui::Outline) {
        const float shortest = std::max(1.0f, std::min(rectangle.width, rectangle.height));
        const float roundness = std::min(1.0f, radius * 2.0f / shortest);

        const Rectangle shadow{rectangle.x - 1.0f, rectangle.y + 2.0f, rectangle.width + 2.0f, rectangle.height + 3.0f};
        DrawRectangleRounded(shadow, roundness, 16, Color{0, 0, 0, 70});
        DrawRectangleRounded(rectangle, roundness, 16, fill);
        DrawRectangleRoundedLinesEx(rectangle, roundness, 16, 1.0f, outline);
    }

    static Color LevelColor(const std::size_t level) {
        switch (level) {
            case 0:
            case 1: return Color{0, 0, 0, 0};
            case 2: return Color{110, 200, 255, 255};
            case 3: return Color{255, 200, 90, 255};
            case 4: return Color{200, 130, 255, 255};
            default: return Color{255, 245, 220, 255};
        }
    }

    static float LevelIntensity(const std::size_t level) {
        if (level <= 1) {
            return 0.0f;
        }
        return std::min(0.25f + 0.2f * static_cast<float>(level - 2), 0.9f);
    }

    static Vector3 ColorVector(const Color color, const float scale) {
        return {
            static_cast<float>(color.r) / 255.0f * scale,
            static_cast<float>(color.g) / 255.0f * scale,
            static_cast<float>(color.b) / 255.0f * scale
        };
    }

    static Color HealthColor(const float ratio) {
        if (ratio > 0.6f) {
            return Color{94, 214, 132, 255};
        }
        if (ratio > 0.3f) {
            return Color{246, 196, 82, 255};
        }
        return Color{238, 92, 92, 255};
    }

    static void centerCamera(Camera3D &camera, const gui::MatrixConnection::MatrixSnapshot &matrix) {
        const float span = static_cast<float>(std::max(matrix.width, matrix.height)) * zioop::CELL_SIZE;
        camera.target = {
            static_cast<float>(matrix.width - 1) * zioop::CELL_SIZE / 2.0f, 0.0f,
            static_cast<float>(matrix.height - 1) * zioop::CELL_SIZE / 2.0f
        };
        const float distance = std::max(span, 4.0f);
        camera.position = {
            camera.target.x + distance * 0.85f, distance,
            camera.target.z + distance * 0.85f
        };
    }

    static Vector2 PoseHeightRange(const Model &model) {
        Vector2 range{std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()};
        for (int m = 0; m < model.meshCount; ++m) {
            const Mesh &mesh = model.meshes[m];
            const float *vertices = mesh.animVertices != nullptr ? mesh.animVertices : mesh.vertices;
            if (vertices == nullptr) {
                continue;
            }
            for (int v = 0; v < mesh.vertexCount; ++v) {
                const Vector3 point = Vector3Transform(
                    {vertices[v * 3], vertices[v * 3 + 1], vertices[v * 3 + 2]}, model.transform);
                range.x = std::min(range.x, point.y);
                range.y = std::max(range.y, point.y);
            }
        }
        return range;
    }

    static void GroundMesh(Renderer::ModelRender &mesh) {
        const bool modelReady = IsModelValid(mesh.model);

        if (mesh.modelReady) {
            return;
        }

        const auto bounds = GetModelBoundingBox(mesh.model);
        const float size = std::max({
            bounds.max.x - bounds.min.x,
            bounds.max.y - bounds.min.y,
            bounds.max.z - bounds.min.z
        });
        if (size > 0.0f) {
            mesh.scale = 0.45f / size;
        }

        Vector2 range{bounds.min.y, bounds.max.y};
        const int restPose = mesh.idleAnimation >= 0 ? mesh.idleAnimation : mesh.walkAnimation;
        if (restPose >= 0) {
            UpdateModelAnimation(mesh.model, mesh.animations[restPose], 0);
            range = PoseHeightRange(mesh.model);
        }

        mesh.baseY = -range.x * mesh.scale;
        mesh.height = (range.y - range.x) * mesh.scale;
        mesh.modelReady = modelReady;
    }

    static int FindAnimation(
        const Renderer::ModelRender &mesh,
        const std::string_view name
    ) {
        if (mesh.animationCount == 0) {
            return -1;
        }

        for (int i = 0; i < mesh.animationCount; ++i) {
            const ModelAnimation &animation = mesh.animations[i];

            if (
                std::string_view(animation.name) == name &&
                animation.frameCount > 0 &&
                IsModelAnimationValid(mesh.model, animation)
            ) {
                return i;
            }
        }

        std::cerr << "Animation '" << name << "' not found or invalid\n";
        return -1;
    }

    static void LoadModel(
        Renderer::ModelRender &mesh,
        const std::string_view &name
    ) {
        const auto asset = (
            std::filesystem::path(GetApplicationDirectory())
            / "assets"
            / std::format("{}", name)
        ).string();

        if (!FileExists(asset.c_str())) {
            throw std::runtime_error("Unable to load GLTF file.");
        }

        mesh.model = ::LoadModel(asset.c_str());
        if (!IsModelValid(mesh.model)) {
            throw std::runtime_error("Unable to load GLTF model.");
        }
        correctMaterialColors(mesh.model);

        mesh.animations =
                ::LoadModelAnimations(
                    asset.c_str(),
                    &mesh.animationCount
                );

        mesh.walkAnimation = FindAnimation(mesh, "Walk");
        mesh.idleAnimation = FindAnimation(mesh, "Idle");
        mesh.attackAnimation = FindAnimation(mesh, "Bite_Front");
        mesh.takeAnimation = FindAnimation(mesh, "Bite_InPlace");
        mesh.ritualAnimation = FindAnimation(mesh, "Dance");
        mesh.levelUpAnimation = FindAnimation(mesh, "Jump");
        mesh.deathAnimation = FindAnimation(mesh, "Death");

        GroundMesh(mesh);
    }

    static int ActionAnimation(
        const Renderer::ModelRender &mesh,
        const std::string &action
    ) {
        if (action == "ATTACK") {
            return mesh.attackAnimation;
        }
        if (action == "TAKE") {
            return mesh.takeAnimation;
        }
        if (action == "RITUAL") {
            return mesh.ritualAnimation;
        }
        return -1;
    }

    // Joue une animation une seule fois ; la relance si elle est déjà en cours.
    static void PlayOnce(Renderer::AgentMotion &motion, const int animation) {
        if (animation < 0) {
            return;
        }
        motion.actionAnimation = animation;
        if (motion.animation == animation) {
            motion.frame = 0;
            motion.animationTimer = 0.0f;
        }
    }

    Renderer::Renderer(
        const int width,
        const int height,
        std::string title
    ) : m_width(width),
        m_height(height),
        m_title(std::move(title)) {
        std::cout << "Creating renderer width=" << width << " height=" << height << std::endl;
        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
        InitWindow(m_width, m_height, m_title.c_str());
        SetTargetFPS(60);

        m_moveKeys.clear();
        for (const int key: {KEY_W, KEY_A, KEY_S, KEY_D}) {
            const char *name = glfwGetKeyName(key, 0);
            m_moveKeys += name != nullptr && name[0] != '\0'
                              ? static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])))
                              : static_cast<char>(key);
        }

        m_lighting = std::make_unique<Lighting>();
        m_terrain.loadProps();

        Image glow = GenImageGradientRadial(64, 64, 0.0f, WHITE, BLANK);
        m_glow = LoadTextureFromImage(glow);
        UnloadImage(glow);
        SetTextureFilter(m_glow, TEXTURE_FILTER_BILINEAR);

        ModelRender &agentModelRender = m_agentModel;

        struct ModelOptions {
            float scale;
            float zOffset;
        };

        struct ModelResource {
            core::ResourceType type;
            std::string_view path; // <-- Remplacement de std::string
            ModelOptions options;
        };

        // constexpr garantit que le tableau ne coûte rien à l'exécution
        constexpr std::array<ModelResource, 4> model_resources = {{
            {
                .type = core::ResourceType::APPLE,
                .path = "apple/scene.gltf",
                .options = {.scale = .003f, .zOffset = .095f}
            },{
                .type = core::ResourceType::CAKE,
                .path = "cake/scene.gltf",
                .options = {.scale = .010f, .zOffset = .095f}
            },{
                .type = core::ResourceType::MEAT,
                .path = "meat/scene.gltf",
                .options = {.scale = .010f, .zOffset = .095f}
            },{
                .type = core::ResourceType::MAGIC_WATER,
                .path = "water/scene.gltf",
                .options = {.scale = .10f, .zOffset = .095f}
            }
        }};

        for (const auto& model : model_resources) {
            ResourceModel newResource{
                .scale = model.options.scale,
                .zOffset = model.options.zOffset
            };

            LoadModel(newResource.resource, model.path);
            newResource.minY = PoseHeightRange(newResource.resource.model).x;
            m_resourceModels.emplace(static_cast<int>(model.type), newResource);
        }

        const std::string path =
                std::string(GetApplicationDirectory()) + "assets/sol.jpg";

        m_groundTexture = LoadTexture(path.c_str());

        LoadModel(agentModelRender, "Alien.gltf");

        const std::pair<core::ResourceType, const char *> inventoryIcons[] = {
            {core::ResourceType::MEAT, "meat.png"},
            {core::ResourceType::CAKE, "cake.png"},
            {core::ResourceType::APPLE, "fruit.png"}
        };
        for (const auto &[type, filename]: inventoryIcons) {
            const auto iconPath = (
                std::filesystem::path(GetApplicationDirectory()) / "assets" / "icons" / filename
            ).string();
            const auto texture = LoadTexture(iconPath.c_str());
            if (IsTextureValid(texture)) {
                SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
                m_inventoryTextures.emplace(static_cast<int>(type), texture);
            }
        }

        DisableCursor();
    }

    static Matrix PlacementTransform(const Vector3 position, const float rotationDegrees, const float scale) {
        return MatrixMultiply(
            MatrixMultiply(MatrixScale(scale, scale, scale), MatrixRotateY(rotationDegrees * DEG2RAD)),
            MatrixTranslate(position.x, position.y, position.z)
        );
    }

    void Renderer::DrawAgents() const {
        const auto &model = m_agentModel;
        const bool colorPass = m_lighting->pass() == Lighting::Pass::Color;

        for (const auto &[id, agent]: m_matrix.agents) {
            const auto it = m_agentMotions.find(id);

            const AgentMotion *motion =
                    it != m_agentMotions.end()
                        ? &it->second
                        : nullptr;

            Vector3 visualPosition =
                    motion ? motion->current : agent.position;
            visualPosition.y = m_terrain.heightAt(visualPosition.x, visualPosition.z);

            const Color color = AgentColor(id);

            if (colorPass) {
                const Vector3 ring{visualPosition.x, visualPosition.y + 0.004f, visualPosition.z};
                DrawCylinder(ring, 0.19f, 0.19f, 0.008f, 24, color);
                DrawCylinderWires(ring, 0.21f, 0.21f, 0.009f, 24, ColorBrightness(color, 0.4f));
            }

            auto position = visualPosition;

            if (model.modelReady) {
                position.y += model.baseY;

                const float rotation = motion ? motion->rotation : 0.0f;

                if (motion && motion->animation >= 0) {
                    UpdateModelAnimation(
                        model.model,
                        model.animations[motion->animation],
                        motion->frame
                    );
                }

                const float intensity = LevelIntensity(agent.level);
                if (colorPass && intensity > 0.0f) {
                    const float pulse = 0.85f + 0.15f * std::sin(m_elapsed * 3.0f + static_cast<float>(id));
                    m_lighting->setGlow(ColorVector(LevelColor(agent.level), intensity * pulse));
                }
                m_lighting->drawModel(model.model, PlacementTransform(position, rotation, model.scale));
                if (colorPass && intensity > 0.0f) {
                    m_lighting->setGlow({0.0f, 0.0f, 0.0f});
                }
            } else if (colorPass) {
                position.y += 0.2f;
                DrawCube(position, 0.25f, 0.4f, 0.25f, color);
            }
        }

        if (model.modelReady && model.deathAnimation >= 0) {
            for (const auto &corpse: m_corpses) {
                UpdateModelAnimation(
                    model.model,
                    model.animations[model.deathAnimation],
                    corpse.frame
                );

                auto position = corpse.position;
                position.y = m_terrain.heightAt(position.x, position.z) + model.baseY;

                m_lighting->drawModel(model.model, PlacementTransform(position, corpse.rotation, model.scale), GRAY);
            }
        }
    }

    void Renderer::collectAgentLights(std::vector<PointLight> &lights, const float night) const {
        for (const auto &[id, motion]: m_agentMotions) {
            const Color color = AgentColor(id);
            const Vector3 position{
                motion.current.x,
                m_terrain.heightAt(motion.current.x, motion.current.z) + 0.25f,
                motion.current.z
            };

            if (motion.ritual) {
                const float pulse = 0.75f + 0.25f * std::sin(motion.ritualTime * 6.0f);
                lights.push_back({
                    .position = Vector3Add(position, {0.0f, 0.35f, 0.0f}),
                    .color = Vector3Scale({0.75f, 0.45f, 1.0f}, (1.5f + 2.5f * night) * pulse),
                    .radius = 3.0f,
                    .priority = true
                });
            }

            if (motion.level >= 3) {
                lights.push_back({
                    .position = Vector3Add(position, {0.0f, 0.2f, 0.0f}),
                    .color = ColorVector(LevelColor(motion.level),
                                         (0.5f + 0.4f * static_cast<float>(motion.level - 3)) * (0.6f + 0.6f * night)),
                    .radius = 1.8f
                });
            }

            if (motion.burst > 0.0f) {
                const float fade = motion.burst / 1.5f;
                lights.push_back({
                    .position = Vector3Add(position, {0.0f, 0.5f, 0.0f}),
                    .color = Vector3Scale({1.0f, 0.85f, 0.45f}, 6.0f * fade * fade),
                    .radius = 5.0f,
                    .priority = true
                });
            }

            if (motion.lookTime >= 0.0f) {
                const float length = LookLength(motion.level);
                const float extend = std::min(motion.lookTime / 0.25f, 1.0f);
                const Vector3 eye = eyePosition(motion);
                lights.push_back({
                    .position = Vector3Add(eye, Vector3Scale(motion.lookDirection, length * 0.6f * extend)),
                    .color = Vector3Scale({
                        0.35f + 0.65f * static_cast<float>(color.r) / 255.0f,
                        0.35f + 0.65f * static_cast<float>(color.g) / 255.0f,
                        0.35f + 0.65f * static_cast<float>(color.b) / 255.0f
                    }, (1.2f + 2.0f * night) * LookFade(motion.lookTime)),
                    .radius = length + 0.8f
                });
            }

            if (night < 0.05f) {
                continue;
            }
            lights.push_back({
                .position = position,
                .color = Vector3Scale({
                    static_cast<float>(color.r) / 255.0f,
                    static_cast<float>(color.g) / 255.0f,
                    static_cast<float>(color.b) / 255.0f
                }, 0.9f * night),
                .radius = 1.4f
            });
        }
    }

    Vector3 Renderer::eyePosition(const AgentMotion &motion) const {
        return {
            motion.current.x + motion.lookDirection.x * 0.06f,
            m_terrain.heightAt(motion.current.x, motion.current.z) + 0.05f + m_agentModel.height * 0.7f,
            motion.current.z + motion.lookDirection.z * 0.06f
        };
    }

    void Renderer::DrawLevelAuras() const {
        const bool active = std::ranges::any_of(m_matrix.agents | std::views::values, [](const auto &agent) {
            return agent.level >= 2;
        });
        if (!active) {
            return;
        }

        constexpr Vector3 axis{1.0f, 0.0f, 0.0f};

        rlDrawRenderBatchActive();
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthMask();

        for (const auto &[id, agent]: m_matrix.agents) {
            if (agent.level < 2) {
                continue;
            }
            const auto motion = m_agentMotions.find(id);
            if (motion == m_agentMotions.end()) {
                continue;
            }

            const auto tier = static_cast<float>(agent.level - 1);
            const Color color = LevelColor(agent.level);
            const float seed = static_cast<float>(id);
            const float pulse = 0.8f + 0.2f * std::sin(m_elapsed * 3.0f + seed);
            const float height = m_agentModel.height;
            const Vector3 base{
                motion->second.current.x,
                m_terrain.heightAt(motion->second.current.x, motion->second.current.z) + 0.01f,
                motion->second.current.z
            };
            const Vector3 body{base.x, base.y + height * 0.5f, base.z};

            DrawBillboard(m_camera, m_glow, body, (0.55f + 0.15f * tier) * pulse,
                          Fade(color, std::min(0.12f + 0.06f * tier, 0.4f)));

            for (int ring = 0; ring < static_cast<int>(tier) && ring < 3; ++ring) {
                const float wave = std::fmod(m_elapsed * 0.5f + static_cast<float>(ring) / 3.0f + seed * 0.13f, 1.0f);
                DrawCircle3D(base, 0.18f + wave * 0.25f, axis, 90.0f, Fade(color, (1.0f - wave) * 0.7f));
            }

            if (agent.level >= 3) {
                const int sparkles = 3 * static_cast<int>(agent.level - 2) + 3;
                for (int k = 0; k < sparkles; ++k) {
                    const float phase = static_cast<float>(k) / static_cast<float>(sparkles);
                    const float angle = m_elapsed * (1.2f + 0.2f * tier) + phase * 2.0f * PI;
                    const Vector3 sparkle{
                        body.x + std::cos(angle) * 0.28f,
                        body.y + std::sin(m_elapsed * 2.0f + phase * 6.0f) * height * 0.35f,
                        body.z + std::sin(angle) * 0.28f
                    };
                    DrawBillboard(m_camera, m_glow, sparkle, 0.07f, Fade(color, 0.9f));
                }

                const Vector3 crown{base.x, base.y + height + 0.03f + 0.015f * std::sin(m_elapsed * 2.0f + seed), base.z};
                for (int line = 0; line < 3; ++line) {
                    DrawCircle3D(crown, 0.09f + 0.004f * static_cast<float>(line), axis, 90.0f, Fade(color, 0.85f * pulse));
                }
                DrawBillboard(m_camera, m_glow, crown, 0.22f * pulse, Fade(color, 0.45f));
            }

            if (agent.level >= 4) {
                DrawCylinder(base, 0.14f, 0.02f, 1.3f, 16, Fade(color, 0.07f * pulse));
                for (int k = 0; k < 8; ++k) {
                    const float offset = hash01(static_cast<int>(id), k, 977);
                    const float life = std::fmod(m_elapsed * 0.35f + offset, 1.0f);
                    const Vector3 mote{
                        base.x + std::cos(offset * 40.0f) * 0.12f,
                        base.y + life * 1.3f,
                        base.z + std::sin(offset * 40.0f) * 0.12f
                    };
                    DrawBillboard(m_camera, m_glow, mote, 0.06f, Fade(color, (1.0f - life) * 0.8f));
                }
            }
        }

        EndBlendMode();
        rlEnableDepthMask();
    }

    void Renderer::DrawLookBeams() const {
        const bool active = std::ranges::any_of(m_agentMotions | std::views::values, [](const AgentMotion &motion) {
            return motion.lookTime >= 0.0f;
        });
        if (!active) {
            return;
        }

        rlDrawRenderBatchActive();
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthMask();

        for (const auto &[id, motion]: m_agentMotions) {
            if (motion.lookTime < 0.0f) {
                continue;
            }

            const float fade = LookFade(motion.lookTime);
            const float extend = std::min(motion.lookTime / 0.25f, 1.0f);
            const float length = LookLength(motion.level) * extend;
            const float spread = std::max(0.25f, static_cast<float>(motion.level) - 0.6f) * 0.7f * extend;
            const Color tint = ColorBrightness(AgentColor(id), 0.35f);

            const Vector3 eye = eyePosition(motion);
            const float groundAtEnd = m_terrain.heightAt(
                eye.x + motion.lookDirection.x * length,
                eye.z + motion.lookDirection.z * length
            );
            const Vector3 end{
                eye.x + motion.lookDirection.x * length,
                std::max(groundAtEnd + 0.08f, eye.y - 0.25f),
                eye.z + motion.lookDirection.z * length
            };

            DrawCylinderEx(eye, end, 0.02f, spread, 24, Fade(tint, 0.10f * fade));
            DrawCylinderEx(eye, end, 0.01f, spread * 0.45f, 16, Fade(WHITE, 0.10f * fade));

            const float sweep = std::fmod(motion.lookTime * 1.6f, 1.0f);
            DrawBillboard(m_camera, m_glow, Vector3Lerp(eye, end, sweep), 0.25f + spread * sweep,
                          Fade(tint, 0.35f * fade));

            DrawBillboard(m_camera, m_glow, {end.x, groundAtEnd + 0.05f, end.z}, spread * 2.6f,
                          Fade(tint, 0.45f * fade));
            DrawBillboard(m_camera, m_glow, eye, 0.14f, Fade(WHITE, 0.9f * fade));
        }

        EndBlendMode();
        rlEnableDepthMask();
    }

    void Renderer::DrawRitualEffects() const {
        const bool active = std::ranges::any_of(m_agentMotions | std::views::values, [](const AgentMotion &motion) {
            return motion.ritual || motion.burst > 0.0f;
        });
        if (!active) {
            return;
        }

        constexpr Color violet{180, 110, 255, 255};
        constexpr Color gold{255, 210, 120, 255};
        constexpr Vector3 axis{1.0f, 0.0f, 0.0f};

        rlDrawRenderBatchActive();
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthMask();

        for (const auto &[id, motion]: m_agentMotions) {
            const Vector3 base{
                motion.current.x,
                m_terrain.heightAt(motion.current.x, motion.current.z) + 0.06f,
                motion.current.z
            };

            if (motion.ritual) {
                const float time = motion.ritualTime;
                const float appear = std::min(time / 0.8f, 1.0f);
                const float pulse = 0.75f + 0.25f * std::sin(time * 6.0f);

                for (int ring = 0; ring < 3; ++ring) {
                    const float radius = (0.22f + 0.1f * static_cast<float>(ring)) * appear;
                    for (int line = 0; line < 3; ++line) {
                        DrawCircle3D(base, radius + 0.004f * static_cast<float>(line), axis, 90.0f,
                                     Fade(ring == 1 ? gold : violet, 0.8f * pulse));
                    }
                }

                for (int k = 0; k < 8; ++k) {
                    const float angle = time * 1.5f + static_cast<float>(k) * 2.0f * PI / 8.0f;
                    const Vector3 rune{
                        base.x + std::cos(angle) * 0.32f * appear,
                        base.y + 0.02f,
                        base.z + std::sin(angle) * 0.32f * appear
                    };
                    DrawBillboard(m_camera, m_glow, rune, 0.08f, Fade(gold, 0.9f * pulse));
                }

                DrawCylinder(base, 0.16f * appear, 0.04f * appear, 1.6f * appear, 16, Fade(violet, 0.18f * pulse));
                DrawCylinder(base, 0.07f * appear, 0.02f, 2.2f * appear, 12, Fade(gold, 0.22f * pulse));

                for (int k = 0; k < 14; ++k) {
                    const float offset = static_cast<float>(k) / 14.0f;
                    const float life = std::fmod(time * 0.6f + offset, 1.0f);
                    const float angle = life * 4.0f * PI + offset * 2.0f * PI;
                    const float radius = 0.3f * (1.0f - life * 0.7f);
                    const Vector3 particle{
                        base.x + std::cos(angle) * radius,
                        base.y + life * 1.4f,
                        base.z + std::sin(angle) * radius
                    };
                    DrawBillboard(m_camera, m_glow, particle, 0.09f * (1.0f - life * 0.5f),
                                  Fade(k % 2 == 0 ? violet : gold, (1.0f - life) * 0.9f * appear));
                }

                DrawBillboard(m_camera, m_glow, Vector3Add(base, {0.0f, 0.25f, 0.0f}),
                              0.9f * pulse, Fade(violet, 0.35f * appear));
            }

            if (motion.burst > 0.0f) {
                const float progress = 1.0f - motion.burst / 1.5f;
                const float fade = 1.0f - progress;

                for (int ring = 0; ring < 3; ++ring) {
                    DrawCircle3D(base, 0.2f + progress * (2.0f + 0.3f * static_cast<float>(ring)), axis, 90.0f,
                                 Fade(gold, fade * (0.9f - 0.25f * static_cast<float>(ring))));
                }

                DrawBillboard(m_camera, m_glow, Vector3Add(base, {0.0f, 0.35f, 0.0f}),
                              0.6f + progress * 2.5f, Fade(gold, fade * fade));

                for (int k = 0; k < 20; ++k) {
                    const float spread = hash01(static_cast<int>(id), k, 907);
                    const float rise = hash01(static_cast<int>(id), k, 911);
                    const float angle = static_cast<float>(k) * 2.39996f;
                    const Vector3 spark{
                        base.x + std::cos(angle) * progress * 0.9f * spread,
                        base.y + progress * 1.8f * (0.5f + rise),
                        base.z + std::sin(angle) * progress * 0.9f * spread
                    };
                    DrawBillboard(m_camera, m_glow, spark, 0.1f, Fade(k % 3 == 0 ? violet : gold, fade));
                }
            }
        }

        EndBlendMode();
        rlEnableDepthMask();
    }

    void Renderer::renderScene(const float deltaTime) {
        m_elapsed += deltaTime;
        m_dayCycle.update(deltaTime);
        const auto environment = m_dayCycle.environment();

        std::vector<PointLight> lights;
        m_terrain.collectLights(lights, m_elapsed, environment.night);
        collectAgentLights(lights, environment.night);
        m_lighting->setEnvironment(environment, m_camera, std::move(lights));

        BeginDrawing();

        if (m_matrix.width > 0 && m_matrix.height > 0) {
            const Vector3 center{
                static_cast<float>(m_matrix.width - 1) * zioop::CELL_SIZE / 2.0f,
                0.0f,
                static_cast<float>(m_matrix.height - 1) * zioop::CELL_SIZE / 2.0f
            };
            const float radius = static_cast<float>(std::max(m_matrix.width, m_matrix.height)) * 0.5f + 5.0f;

            m_lighting->beginShadowPass(center, radius);
            m_terrain.drawShadowCasters(*m_lighting, m_elapsed);
            DrawAgents();
            DrawResources();
            m_lighting->endShadowPass();
        }

        ClearBackground(BLACK);
        m_lighting->drawSky(m_camera, m_elapsed);

        BeginMode3D(m_camera);
        m_lighting->beginColorPass();
        m_terrain.draw(*m_lighting, m_elapsed);
        DrawAgents();
        DrawResources();
        m_lighting->endColorPass();
        m_terrain.drawGlow(m_camera, m_elapsed, environment.night);
        DrawLevelAuras();
        DrawRitualEffects();
        DrawLookBeams();
        EndMode3D();
    }

    float Renderer::DrawKeyHint(const std::string &key, const std::string &label, const Vector2 position) const {
        constexpr int keySize = 12;
        constexpr int labelSize = 13;
        const Vector2 keyText = m_typography.measure(key, keySize, Weight::SemiBold);
        const Rectangle chip{position.x, position.y, std::max(keyText.x + 12.0f, 22.0f), 20.0f};

        DrawRectangleRounded(chip, 0.4f, 8, ui::SurfaceRaised);
        DrawRectangleRoundedLinesEx(chip, 0.4f, 8, 1.0f, Color{255, 255, 255, 50});
        m_typography.draw(key, {chip.x + (chip.width - keyText.x) / 2.0f, chip.y + (chip.height - keyText.y) / 2.0f},
                          keySize, ui::TextPrimary, Weight::SemiBold);

        const Vector2 labelText = m_typography.measure(label, labelSize);
        m_typography.draw(label, {chip.x + chip.width + 6.0f, chip.y + (chip.height - labelText.y) / 2.0f},
                          labelSize, ui::TextSecondary);
        return chip.width + 6.0f + labelText.x + 14.0f;
    }

    void Renderer::DrawEnvironmentInformation() const {
        const auto environment = m_dayCycle.environment();
        const bool night = environment.night > 0.5f;
        const auto clock = m_dayCycle.clock();
        const auto period = night ? std::string{"Nuit"} : std::string{"Jour"};
        const auto speed = m_dayCycle.paused() ? std::string{"En pause"} : std::format("Vitesse x{:g}", m_dayCycle.speed());

        const std::pair<std::string, std::string> hints[] = {
            {m_moveKeys, "Déplacer"}, {"P", "Pause"}, {"-/+", "Vitesse"}, {"N", "Jour/Nuit"},
            {"M", "Messages"}, {"R", "Recentrer"}
        };
        float hintsWidth = 0.0f;
        for (const auto &[key, label]: hints) {
            hintsWidth += std::max(m_typography.measure(key, 12, Weight::SemiBold).x + 12.0f, 22.0f) + 6.0f
                          + m_typography.measure(label, 13).x + 14.0f;
        }

        const Rectangle panel{12.0f, 12.0f, std::max(hintsWidth + 16.0f, 300.0f), 78.0f};
        DrawPanel(panel, 12.0f, ui::Surface);

        const Vector2 icon{panel.x + 26.0f, panel.y + 25.0f};
        if (night) {
            DrawCircleV(icon, 10.0f, Color{214, 222, 255, 255});
            DrawCircleV({icon.x + 5.0f, icon.y - 4.0f}, 9.0f, ui::Surface);
        } else {
            DrawCircleV(icon, 13.0f, Color{255, 204, 102, 60});
            DrawCircleV(icon, 9.0f, ui::Gold);
        }

        m_typography.draw(clock, {panel.x + 46.0f, panel.y + 9.0f}, 26, ui::TextPrimary, Weight::SemiBold);
        const float clockWidth = m_typography.measure(clock, 26, Weight::SemiBold).x;
        m_typography.draw(period + "  ·  " + speed, {panel.x + 56.0f + clockWidth, panel.y + 17.0f}, 14,
                          ui::TextSecondary, Weight::Medium);

        float x = panel.x + 12.0f;
        for (const auto &[key, label]: hints) {
            x += DrawKeyHint(key, label, {x, panel.y + 48.0f});
        }
    }

    static std::string InventoryResourceName(const int type) {
        switch (static_cast<core::ResourceType>(type)) {
            case core::ResourceType::MEAT: return "MEAT";
            case core::ResourceType::CAKE: return "CAKE";
            case core::ResourceType::APPLE: return "APPLE";
            case core::ResourceType::MAGIC_WATER: return "MAGIC_WATER";
        }
        return std::format("Resource {}", type);
    }

    void Renderer::DrawAgentInformation() const {
        constexpr int titleSize = 14;
        constexpr int textSize = 13;
        constexpr float iconSize = 18.0f;
        constexpr float padding = 9.0f;
        constexpr float itemGap = 10.0f;
        constexpr float headerHeight = 20.0f;
        constexpr float barHeight = 6.0f;
        constexpr float rowGap = 7.0f;
        constexpr float panelHeight = padding + headerHeight + rowGap + barHeight + rowGap + iconSize + padding;
        const auto cameraForward = Vector3Normalize(Vector3Subtract(m_camera.target, m_camera.position));

        for (const auto &[id, agent]: m_matrix.agents) {
            const auto motion = m_agentMotions.find(id);
            auto anchor = motion != m_agentMotions.end() ? motion->second.current : agent.position;
            anchor.y = m_terrain.heightAt(anchor.x, anchor.z) + 0.05f;
            anchor.y += m_agentModel.height + 0.12f;
            if (Vector3DotProduct(Vector3Subtract(anchor, m_camera.position), cameraForward) <= 0.0f) {
                continue;
            }

            const auto screen = GetWorldToScreen(anchor, m_camera);
            if (!std::isfinite(screen.x) || !std::isfinite(screen.y) ||
                screen.x < 0 || screen.x > GetScreenWidth() ||
                screen.y < 0 || screen.y > GetScreenHeight()) {
                continue;
            }

            const auto color = AgentColor(id);
            const auto name = std::format("Agent #{}", id);
            const auto level = std::format("Niv. {}", agent.level);
            const auto health = std::format("{} PV", agent.health);

            struct InventoryEntry {
                Texture2D icon;
                std::string text;
                float width;
            };
            std::vector<InventoryEntry> entries;
            float inventoryWidth = 0.0f;
            for (const auto &item: agent.inventory) {
                if (item.quantity == 0) {
                    continue;
                }
                const auto texture = m_inventoryTextures.find(item.type);
                const auto icon = texture != m_inventoryTextures.end() ? texture->second : Texture2D{};
                const auto text = icon.id != 0
                                      ? std::format("{}", item.quantity)
                                      : std::format("{} {}", InventoryResourceName(item.type), item.quantity);
                const float width = m_typography.measure(text, textSize, Weight::Medium).x +
                                    (icon.id != 0 ? iconSize + 4.0f : 0.0f);
                if (!entries.empty()) {
                    inventoryWidth += itemGap;
                }
                inventoryWidth += width;
                entries.push_back({icon, text, width});
            }
            const std::string emptyLabel = "Inventaire vide";
            if (entries.empty()) {
                inventoryWidth = m_typography.measure(emptyLabel, textSize).x;
            }

            const Vector2 levelSize = m_typography.measure(level, 12, Weight::SemiBold);
            const float levelChipWidth = levelSize.x + 14.0f;
            const float headerWidth = m_typography.measure(name, titleSize, Weight::SemiBold).x + 10.0f + levelChipWidth;
            const float healthWidth = 90.0f + m_typography.measure(health, 12, Weight::Medium).x;
            const float panelWidth = std::max({headerWidth, inventoryWidth, healthWidth, 130.0f}) + padding * 2.0f;
            const Rectangle panel{
                std::round(screen.x - panelWidth / 2.0f),
                std::round(screen.y - panelHeight - 6.0f), panelWidth, panelHeight
            };

            DrawPanel(panel, 10.0f, ui::Surface, Fade(color, 0.55f));
            DrawRectangleRounded({panel.x + 10.0f, panel.y, panel.width - 20.0f, 3.0f}, 1.0f, 4, color);

            float y = panel.y + padding;
            m_typography.drawShadowed(name, {panel.x + padding, y}, titleSize, ColorBrightness(color, 0.45f), Weight::SemiBold);

            const Rectangle levelChip{
                panel.x + panel.width - padding - levelChipWidth, y, levelChipWidth, headerHeight
            };
            DrawRectangleRounded(levelChip, 0.5f, 8, Color{255, 204, 102, 40});
            m_typography.draw(level, {levelChip.x + 7.0f, levelChip.y + (headerHeight - levelSize.y) / 2.0f},
                              12, ui::Gold, Weight::SemiBold);

            y += headerHeight + rowGap;
            const float ratio = std::clamp(static_cast<float>(agent.health) / 100.0f, 0.0f, 1.0f);
            const Vector2 healthSize = m_typography.measure(health, 12, Weight::Medium);
            const float barWidth = panel.width - padding * 2.0f - healthSize.x - 8.0f;
            const Rectangle bar{panel.x + padding, y, barWidth, barHeight};
            DrawRectangleRounded(bar, 1.0f, 8, Color{255, 255, 255, 30});
            if (ratio > 0.0f) {
                DrawRectangleRounded({bar.x, bar.y, std::max(bar.height, bar.width * ratio), bar.height}, 1.0f, 8,
                                     HealthColor(ratio));
            }
            m_typography.draw(health, {bar.x + bar.width + 8.0f, y + (barHeight - healthSize.y) / 2.0f}, 12,
                              ui::TextSecondary, Weight::Medium);

            y += barHeight + rowGap;
            float x = panel.x + padding;
            if (entries.empty()) {
                const Vector2 size = m_typography.measure(emptyLabel, textSize);
                m_typography.draw(emptyLabel, {x, y + (iconSize - size.y) / 2.0f}, textSize, ui::TextSecondary);
            }
            for (const auto &entry: entries) {
                float textX = x;
                if (entry.icon.id != 0) {
                    const float scale = iconSize / static_cast<float>(std::max(entry.icon.width, entry.icon.height));
                    const float width = static_cast<float>(entry.icon.width) * scale;
                    const float height = static_cast<float>(entry.icon.height) * scale;
                    DrawTexturePro(entry.icon,
                                   {0, 0, static_cast<float>(entry.icon.width), static_cast<float>(entry.icon.height)},
                                   {x + (iconSize - width) / 2.0f, y + (iconSize - height) / 2.0f, width, height},
                                   {0, 0}, 0, WHITE);
                    textX += iconSize + 4.0f;
                }
                const Vector2 size = m_typography.measure(entry.text, textSize, Weight::Medium);
                m_typography.draw(entry.text, {textX, y + (iconSize - size.y) / 2.0f}, textSize, ui::TextPrimary,
                                  Weight::Medium);
                x += entry.width + itemGap;
            }

            if (!agent.message.empty()) {
                DrawSpeechBubble(agent.message, {screen.x, panel.y - 4.0f}, color);
            }
        }
    }

    void Renderer::recordMessages(const MatrixConnection::MatrixSnapshot &snapshot) {
        constexpr std::size_t MaxEntries = 200;

        for (const auto &[id, agent]: snapshot.agents) {
            if (agent.message.empty()) {
                continue;
            }
            const auto previous = m_matrix.agents.find(id);
            if (previous != m_matrix.agents.end() && previous->second.message == agent.message) {
                continue;
            }

            const auto now = std::chrono::zoned_time{
                std::chrono::current_zone(),
                std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())
            };
            m_chat.push_back({
                .agentId = id,
                .x = agent.x,
                .y = agent.y,
                .time = std::format("{:%H:%M:%S}", now),
                .message = agent.message
            });
            if (m_chat.size() > MaxEntries) {
                m_chat.pop_front();
            }
        }
    }

    void Renderer::DrawChat() const {
        constexpr float width = 340.0f;
        constexpr float margin = 12.0f;
        constexpr float padding = 14.0f;
        constexpr int titleSize = 18;
        constexpr int headerSize = 12;
        constexpr int messageSize = 14;
        constexpr float lineHeight = 20.0f;
        constexpr float cardPadding = 10.0f;
        constexpr float cardGap = 8.0f;

        const Rectangle panel{
            static_cast<float>(GetScreenWidth()) - width - margin,
            margin,
            width,
            static_cast<float>(GetScreenHeight()) - margin * 2.0f
        };
        DrawPanel(panel, 14.0f, Color{16, 20, 28, 200});

        m_typography.draw("Messages", {panel.x + padding, panel.y + padding}, titleSize, ui::TextPrimary, Weight::SemiBold);
        const auto count = std::format("{}", m_chat.size());
        const Vector2 countSize = m_typography.measure(count, 12, Weight::SemiBold);
        const float titleWidth = m_typography.measure("Messages", titleSize, Weight::SemiBold).x;
        const Rectangle badge{panel.x + padding + titleWidth + 8.0f, panel.y + padding + 3.0f,
                              std::max(countSize.x + 14.0f, 22.0f), 20.0f};
        DrawRectangleRounded(badge, 1.0f, 12, ui::SurfaceRaised);
        m_typography.draw(count, {badge.x + (badge.width - countSize.x) / 2.0f, badge.y + (badge.height - countSize.y) / 2.0f},
                          12, ui::TextSecondary, Weight::SemiBold);
        DrawKeyHint("M", "Masquer", {panel.x + panel.width - padding - 80.0f, panel.y + padding + 3.0f});

        const float top = panel.y + padding + titleSize + 22.0f;
        DrawLineEx({panel.x + padding, top - 8.0f}, {panel.x + panel.width - padding, top - 8.0f}, 1.0f, ui::Outline);

        if (m_chat.empty()) {
            m_typography.draw("Aucun message pour l'instant", {panel.x + padding, top + 4.0f}, messageSize, ui::TextSecondary);
            return;
        }

        const float cardWidth = panel.width - padding * 2.0f;
        const float textWidth = cardWidth - cardPadding * 2.0f - 6.0f;
        float bottom = panel.y + panel.height - padding;

        for (auto it = m_chat.rbegin(); it != m_chat.rend(); ++it) {
            const auto lines = m_typography.wrap(it->message, messageSize, textWidth, 3);
            const float cardHeight = cardPadding * 2.0f + 18.0f + static_cast<float>(lines.size()) * lineHeight;
            const float y = bottom - cardHeight;
            if (y < top) {
                break;
            }

            const auto color = AgentColor(it->agentId);
            const Rectangle card{panel.x + padding, y, cardWidth, cardHeight};
            DrawRectangleRounded(card, std::min(1.0f, 16.0f / cardHeight), 12, Color{255, 255, 255, 10});
            DrawRectangleRounded({card.x, card.y + 6.0f, 3.0f, card.height - 12.0f}, 1.0f, 4, color);

            const float textX = card.x + cardPadding + 6.0f;
            m_typography.draw(std::format("Agent #{}", it->agentId), {textX, y + cardPadding - 2.0f}, headerSize + 1,
                              ColorBrightness(color, 0.35f), Weight::SemiBold);
            const auto meta = std::format("({}, {})  {}", it->x, it->y, it->time);
            const float metaWidth = m_typography.measure(meta, headerSize).x;
            m_typography.draw(meta, {card.x + card.width - cardPadding - metaWidth, y + cardPadding - 1.0f}, headerSize,
                              ui::TextSecondary);

            float lineY = y + cardPadding + 18.0f;
            for (const auto &line: lines) {
                m_typography.draw(line, {textX, lineY}, messageSize, ui::TextPrimary);
                lineY += lineHeight;
            }

            bottom = y - cardGap;
        }
    }

    void Renderer::DrawSpeechBubble(const std::string &message, const Vector2 tip, const Color border) const {
        constexpr int fontSize = 15;
        constexpr float padding = 10.0f;
        constexpr float tail = 9.0f;
        constexpr float maxTextWidth = 230.0f;
        constexpr float lineHeight = 21.0f;

        const auto lines = m_typography.wrap(message, fontSize, maxTextWidth, 3, Weight::Medium);
        float textWidth = 0.0f;
        for (const auto &line: lines) {
            textWidth = std::max(textWidth, m_typography.measure(line, fontSize, Weight::Medium).x);
        }

        const float width = textWidth + padding * 2.0f;
        const float height = static_cast<float>(lines.size()) * lineHeight + padding * 2.0f - 4.0f;
        const Rectangle bubble{
            std::round(tip.x - width / 2.0f),
            std::round(tip.y - tail - height),
            width,
            height
        };

        constexpr Color paper{250, 251, 253, 250};
        DrawPanel(bubble, 12.0f, paper, Fade(border, 0.9f));
        DrawTriangle(
            {tip.x - tail, bubble.y + height - 1.5f},
            {tip.x, tip.y},
            {tip.x + tail, bubble.y + height - 1.5f},
            paper
        );
        DrawLineEx({tip.x - tail, bubble.y + height - 0.5f}, {tip.x, tip.y}, 1.0f, Fade(border, 0.9f));
        DrawLineEx({tip.x + tail, bubble.y + height - 0.5f}, {tip.x, tip.y}, 1.0f, Fade(border, 0.9f));

        float y = bubble.y + padding - 2.0f;
        for (const auto &line: lines) {
            const float lineWidth = m_typography.measure(line, fontSize, Weight::Medium).x;
            m_typography.draw(line, {bubble.x + (bubble.width - lineWidth) / 2.0f, y}, fontSize,
                              Color{24, 28, 38, 255}, Weight::Medium);
            y += lineHeight;
        }
    }

    void Renderer::DrawResources() const {
        for (const auto &resource: m_matrix.resources) {
            const auto it = m_resourceModels.find(resource.type);
            if (it != m_resourceModels.end()) {
                const auto &model = it->second;
                auto position = resource.position;

                // Variation stable : dérivée de la sous-case et du type,
                // donc identique d'une frame à l'autre.
                const int cellX = static_cast<int>(std::lround(position.x * 4.0f));
                const int cellZ = static_cast<int>(std::lround(position.z * 4.0f));
                const auto seed = 100u + static_cast<unsigned>(resource.type);

                position.x += (hash01(cellX, cellZ, seed) - 0.5f) * 0.24f;
                position.z += (hash01(cellX, cellZ, seed + 1) - 0.5f) * 0.24f;
                const float angle = hash01(cellX, cellZ, seed + 2) * 360.0f;
                const float scale = model.scale * (0.85f + 0.3f * hash01(cellX, cellZ, seed + 3));
                position.y = m_terrain.heightAt(position.x, position.z) - model.minY * scale;

                m_lighting->drawModel(model.resource.model, PlacementTransform(position, angle, scale));
            }
        }
    }

    void Renderer::updateAgentMotions(
        ModelRender &model,
        const float deltaTime
    ) {
        constexpr float duration = 0.70f;
        constexpr float frameDuration = 1.0f / 60.0f;

        for (auto &motion: m_agentMotions | std::views::values) {
            motion.elapsed += deltaTime;
            motion.ritualTime = motion.ritual ? motion.ritualTime + deltaTime : 0.0f;
            motion.burst = std::max(0.0f, motion.burst - deltaTime);
            if (motion.lookTime >= 0.0f) {
                motion.lookTime += deltaTime;
                if (motion.lookTime > LookDuration) {
                    motion.lookTime = -1.0f;
                }
            }

            const float t = std::clamp(
                motion.elapsed / duration,
                0.0f,
                1.0f
            );

            motion.current = {
                motion.from.x + (motion.target.x - motion.from.x) * t,
                motion.from.y + (motion.target.y - motion.from.y) * t,
                motion.from.z + (motion.target.z - motion.from.z) * t
            };

            const bool moving = motion.elapsed < duration;
            // Priorité : marche, animation ponctuelle, rituel, sinon idle.
            int animation = model.idleAnimation;
            if (moving) {
                animation = model.walkAnimation;
            } else if (motion.actionAnimation >= 0) {
                animation = motion.actionAnimation;
            } else if (motion.ritual && model.ritualAnimation >= 0) {
                animation = model.ritualAnimation;
            }

            if (animation != motion.animation) {
                motion.animation = animation;
                motion.frame = 0;
                motion.animationTimer = 0.0f;
            }

            if (animation < 0) {
                continue;
            }

            const int frameCount = model.animations[animation].frameCount;
            const bool playOnce = !moving && animation == motion.actionAnimation;

            motion.animationTimer += deltaTime;

            while (motion.animationTimer >= frameDuration) {
                motion.animationTimer -= frameDuration;
                if (playOnce && motion.frame + 1 >= frameCount) {
                    motion.actionAnimation = -1; // Action terminée : retour à idle.
                    break;
                }
                motion.frame = (motion.frame + 1) % frameCount;
            }
        }

        // Les cadavres jouent Death une fois, puis restent sur la dernière image.
        if (model.deathAnimation >= 0) {
            const int lastFrame = model.animations[model.deathAnimation].frameCount - 1;
            for (auto &corpse: m_corpses) {
                if (corpse.frame >= lastFrame) {
                    continue;
                }
                corpse.animationTimer += deltaTime;
                while (corpse.frame < lastFrame && corpse.animationTimer >= frameDuration) {
                    corpse.animationTimer -= frameDuration;
                    ++corpse.frame;
                }
            }
        }
    }

    void Renderer::updateMatrix(
        bool &hasSnapshot,
        Clock::time_point &nextRequest
    ) {
        const auto now = Clock::now();
        auto &matrixRequest = m_matrixRequest;
        auto &matrix = m_matrix;
        // auto &MatrixConnection = m_connection;

        if (matrixRequest.valid() &&
            matrixRequest.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            try {
                auto snapshot = matrixRequest.get();

                const bool resized =
                        !hasSnapshot ||
                        matrix.width != snapshot.width ||
                        matrix.height != snapshot.height;

                // Cases où un agent vient d'attaquer : ceux qui y disparaissent sont morts.
                std::set<std::pair<std::size_t, std::size_t> > attackCells;

                for (const auto &[id, agent]: snapshot.agents) {
                    auto [it, inserted] = m_agentMotions.try_emplace(id);
                    auto &motion = it->second;

                    // Spawn inconue sur la matrice.
                    if (inserted) {
                        motion.from = agent.position;
                        motion.target = agent.position;
                        motion.current = agent.position;
                        // Ne pas rejouer une action faite avant l'arrivée de la GUI.
                        motion.actionCount = agent.actionCount;
                    } else {
                        const auto &target = agent.position;
                        const bool move =
                                target.x != motion.target.x ||
                                target.y != motion.target.y ||
                                target.z != motion.target.z;
                        if (move) {
                            const float dx = target.x - motion.current.x;
                            const float dz = target.z - motion.current.z;
                            if (dx != 0.0f || dz != 0.0f) {
                                motion.rotation = std::atan2(dx, dz) * RAD2DEG;
                            }

                            motion.from = motion.current;
                            motion.target = target;
                            motion.elapsed = 0.0f;
                            motion.frame = 0;
                            motion.animationTimer = 0.0f;
                        }

                        if (agent.actionCount != motion.actionCount) {
                            motion.actionCount = agent.actionCount;
                            if (agent.action == "ATTACK") {
                                attackCells.insert({agent.x, agent.y});
                            }
                            PlayOnce(motion, ActionAnimation(m_agentModel, agent.action));
                        }

                        // Niveau connu qui augmente : fin du rituel.
                        if (motion.level > 0 && agent.level > motion.level) {
                            PlayOnce(motion, m_agentModel.levelUpAnimation);
                            motion.burst = 1.5f;
                        }
                    }
                    motion.level = agent.level;
                    motion.ritual = agent.ritual;

                    if (inserted) {
                        motion.lookCount = agent.lookCount;
                    } else if (agent.lookDirection && agent.lookCount != motion.lookCount) {
                        motion.lookCount = agent.lookCount;
                        motion.lookDirection = LookVector(*agent.lookDirection);
                        motion.lookTime = 0.0f;
                        motion.rotation = std::atan2(motion.lookDirection.x, motion.lookDirection.z) * RAD2DEG;
                    }
                }

                constexpr std::size_t MaxCorpses = 64;
                for (auto it = m_agentMotions.begin(); it != m_agentMotions.end();) {
                    if (snapshot.agents.contains(it->first)) {
                        ++it;
                        continue;
                    }
                    // Disparu pendant une attaque sur sa case : il est mort, on garde le cadavre.
                    // Sinon c'est une déconnexion : il disparaît simplement.
                    const auto previous = matrix.agents.find(it->first);
                    if (previous != matrix.agents.end() &&
                        attackCells.contains({previous->second.x, previous->second.y})) {
                        m_corpses.push_back({
                            .position = it->second.current,
                            .rotation = it->second.rotation
                        });
                        if (m_corpses.size() > MaxCorpses) {
                            m_corpses.erase(m_corpses.begin());
                        }
                    }
                    it = m_agentMotions.erase(it);
                }

                recordMessages(snapshot);
                matrix = std::move(snapshot);

                if (resized) {
                    m_corpses.clear(); // Nouvelle partie.
                    centerCamera(m_camera, matrix);
                }
                hasSnapshot = true;
            } catch (const std::exception &error) {
                std::cerr << error.what() << '\n';
                nextRequest = Clock::now() + std::chrono::seconds(2);
            }
        }

        if (!matrixRequest.valid() && now >= nextRequest) {
            matrixRequest = std::async(std::launch::async, [this] {
                return m_connection.request();
            });

            nextRequest = now + zioop::REFRESH_INTERVAL;
        }
    }

    void Renderer::updateCamera(const float deltaTime) {
        constexpr float speed = 4.0f;
        constexpr float sensitivity = 0.12f;

        const float boost = IsKeyDown(KEY_LEFT_SHIFT) ? 2.5f : 1.0f;
        const float step = speed * boost * deltaTime;

        const auto axis = [](const int positive, const int alternatePositive, const int negative, const int alternateNegative) {
            return static_cast<float>(IsKeyDown(positive) || IsKeyDown(alternatePositive))
                   - static_cast<float>(IsKeyDown(negative) || IsKeyDown(alternateNegative));
        };

        const Vector3 movement{
            axis(KEY_W, KEY_UP, KEY_S, KEY_DOWN) * step + GetMouseWheelMove() * 0.6f,
            axis(KEY_D, KEY_RIGHT, KEY_A, KEY_LEFT) * step,
            axis(KEY_SPACE, KEY_PAGE_UP, KEY_LEFT_CONTROL, KEY_PAGE_DOWN) * step
        };
        const Vector2 mouse = GetMouseDelta();
        CameraYaw(&m_camera, -mouse.x * sensitivity * DEG2RAD, false);
        CameraPitch(&m_camera, -mouse.y * sensitivity * DEG2RAD, true, false, false);
        CameraMoveForward(&m_camera, movement.x, false);
        CameraMoveRight(&m_camera, movement.y, false);
        CameraMoveUp(&m_camera, movement.z);
    }

    void Renderer::drawWorld() {
        bool hasSnapshot = false;
        auto nextRequest = Clock::now();
        auto &model = m_agentModel;

        while (!WindowShouldClose()) {
            const float deltaTime = GetFrameTime();
            updateMatrix(hasSnapshot, nextRequest);

            m_terrain.ensure(m_matrix.width, m_matrix.height, m_groundTexture);

            updateAgentMotions(model, deltaTime);

            for (int character = GetCharPressed(); character != 0; character = GetCharPressed()) {
                switch (character) {
                    case 'r':
                    case 'R':
                        if (hasSnapshot) {
                            centerCamera(m_camera, m_matrix);
                        }
                        break;
                    case 'm':
                    case 'M':
                        m_showChat = !m_showChat;
                        break;
                    case 'p':
                    case 'P':
                        m_dayCycle.togglePause();
                        break;
                    case 'n':
                    case 'N':
                        m_dayCycle.advance(12.0f);
                        break;
                    case '+':
                        m_dayCycle.faster();
                        break;
                    case '-':
                        m_dayCycle.slower();
                        break;
                    default:
                        break;
                }
            }

            if (IsKeyPressed(KEY_KP_ADD)) {
                m_dayCycle.faster();
            }

            if (IsKeyPressed(KEY_KP_SUBTRACT)) {
                m_dayCycle.slower();
            }

            updateCamera(deltaTime);

            renderScene(deltaTime);

            DrawAgentInformation();
            DrawEnvironmentInformation();
            if (m_showChat) {
                DrawChat();
            }
            EndDrawing();
        }
    }

    Renderer::~Renderer() {
        EnableCursor();
        if (m_agentModel.animations) {
            UnloadModelAnimations(m_agentModel.animations, m_agentModel.animationCount);
        }
        if (m_agentModel.model.meshes || m_agentModel.model.materials) {
            UnloadModel(m_agentModel.model);
        }

        for (auto &val: m_resourceModels | std::views::values) {
            UnloadModel(val.resource.model);
        }

        m_terrain.unload();
        m_lighting.reset();
        UnloadTexture(m_groundTexture);
        UnloadTexture(m_glow);
        m_typography.unload();
        for (const auto &texture: m_inventoryTextures | std::views::values) {
            UnloadTexture(texture);
        }

        CloseWindow();
    }
}
