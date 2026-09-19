#include "zioop/gui/Renderer.hpp"

#include <iostream>
#include <string>
#include <algorithm>

namespace gui {
    using Clock = std::chrono::steady_clock;

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
        mesh.baseY = -bounds.min.y * mesh.scale;
        mesh.modelReady = modelReady;
    }

    static void LoadModel(
        Renderer::ModelRender &mesh,
        const std::string &name
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

        mesh.animations =
                ::LoadModelAnimations(
                    asset.c_str(),
                    &mesh.animationCount
                );
        mesh.currentAnimation = 0;
        GroundMesh(mesh);
    }

    Renderer::Renderer(
        const int width,
        const int height,
        std::string title
    ) : m_width(width),
        m_height(height),
        m_title(std::move(title)) {
        std::cout << "Creating renderer width=" << width << " height=" << height << std::endl;
        InitWindow(m_width, m_height, m_title.c_str());
        SetTargetFPS(60);

        ModelRender &agentModelRender = m_agentModel;
        ModelRender &resourceModelRender = m_resourceModel;

        const std::string path =
                std::string(GetApplicationDirectory()) + "assets/sol.jpg";

        m_groundTexture = LoadTexture(path.c_str());

        m_groundModel = LoadModelFromMesh(
            GenMeshCube(zioop::CELL_SIZE, 0.05f, zioop::CELL_SIZE)
        );

        m_groundModel.materials[0]
                .maps[MATERIAL_MAP_DIFFUSE]
                .texture = m_groundTexture;

        LoadModel(agentModelRender, "Alien.gltf");
        LoadModel(resourceModelRender, "fruit.gltf");

        DisableCursor();
    }

    void Renderer::DrawAgents() {
        auto &matrix = m_matrix;
        auto &model = m_agentModel;

        for (const auto &[id, agent]: matrix.agents) {
            const auto it = m_agentMotions.find(id);

            const Vector3 visualPosition =
                    it != m_agentMotions.end()
                        ? it->second.current
                        : agent.position;

            const Color color = ColorFromHSV(
                static_cast<float>((id % 12) * 30),
                0.7f,
                0.8f
            );

            DrawCylinder(
                visualPosition,
                0.19f, 0.19f, 0.015f,
                16, color
            );

            auto position = visualPosition;

            if (model.modelReady) {
                position.y += model.baseY;

                const float rotation =
                    it != m_agentMotions.end()
                        ? it->second.rotation
                        : 0.0f;

                DrawModelEx(
                    model.model,
                    position,
                    Vector3{0.0f, 1.0f, 0.0f},
                    rotation,
                    Vector3{model.scale, model.scale, model.scale},
                    WHITE
                );
            } else {
                position.y += 0.2f;
                DrawCube(position, 0.25f, 0.4f, 0.25f, color);
            }
        }
    }

    void Renderer::DrawResources() const {
        auto &matrix = m_matrix;
        auto &model = m_resourceModel;

        for (const auto &resource: matrix.resources) {
            if (IsModelValid(model.model)) {
                auto position = resource.position;
                position.y -= .095f;
                DrawModel(
                    model.model,
                    position,
                    0.3f, // Échelle à adapter à ton asset.
                    WHITE
                );
            }
        }
    }

    void Renderer::updateAgentMotions(float deltaTime) {
        constexpr float duration = 0.70f;

        for (auto &motion: m_agentMotions | std::views::values) {
            motion.elapsed += deltaTime;

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

                for (const auto &[id, agent]: snapshot.agents) {
                    auto [it, inserted] = m_agentMotions.try_emplace(id);
                    auto &motion = it->second;

                    // Spawn inconue sur la matrice.
                    if (inserted) {
                        motion.from = agent.position;
                        motion.target = agent.position;
                        motion.current = agent.position;
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
                        }
                    }
                }

                std::erase_if(m_agentMotions, [&snapshot](const auto &entry) {
                    return snapshot.agents.find(entry.first) == snapshot.agents.end();
                });

                matrix = std::move(snapshot);

                if (resized) {
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

    void Renderer::DrawGround() {
        if (m_matrix.width == 0 || m_matrix.height == 0) {
            return;
        }

        const float cellsize = zioop::CELL_SIZE;
        // constexpr float thickness = 0.05f;

        // Un cube texturé par case.
        for (std::size_t x = 0; x < m_matrix.width; ++x) {
            for (std::size_t z = 0; z < m_matrix.height; ++z) {
                const Vector3 position{
                    static_cast<float>(x) * cellsize,
                    0.0,
                    static_cast<float>(z) * cellsize
                };

                DrawModel(m_groundModel, position, 1.0f, WHITE);
            }
        }

        // const float minX = 0.0f - cellsize * 0.5f;
        // const float maxX =
        //     (static_cast<float>(m_matrix.width) - 0.5f) * cellsize;
        //
        // const float minZ = 0.0f - cellsize * 0.5f;
        // const float maxZ =
        //     (static_cast<float>(m_matrix.height) - 0.5f) * cellsize;
        //
        // constexpr float gridY = thickness * 0.5f + 0.002f;

        // for (std::size_t x = 0; x < m_matrix.width; x++) {
        //     const float worldX =
        //         minX + static_cast<float>(x) * cellsize;
        //     DrawLine3D(
        //         Vector3(worldX, gridY, minZ),
        //         Vector3(worldX, gridY, maxZ),
        //         DARKGRAY
        //     );
        // }
        //
        // for (std::size_t y = 0; y < m_matrix.height; y++) {
        //     const float worldZ =
        //         minZ + static_cast<float>(y) * cellsize;
        //     DrawLine3D(
        //         Vector3(minX, gridY, worldZ),
        //         Vector3(maxX, gridY, worldZ),
        //         DARKGRAY
        //     );
        // }
    }

    void Renderer::drawWorld() {
        bool hasSnapshot = false;
        // bool connected = false;
        // std::string matrixStatus = "GET_MATRIX: waiting for server...";
        auto nextRequest = Clock::now();
        // auto lastUpdate = Clock::now();
        auto &model = m_agentModel;

        while (!WindowShouldClose()) {
            const auto now = Clock::now();
            // auto &matrixRequest = m_matrixRequest;
            auto &matrix = m_matrix;

            // Update data every 250ms.
            updateMatrix(hasSnapshot, nextRequest);

            updateAgentMotions(GetFrameTime());

            if (hasSnapshot && IsKeyPressed(KEY_R)) {
                centerCamera(m_camera, matrix);
            }

            UpdateCamera(&m_camera, CAMERA_FREE);

            // Une mise a jour de l'animation par image, quel que soit le nombre d'agents.
            // if (
            //     model.animations && model.animationCount > 0 &&
            //     model.animations[model.currentAnimation].frameCount > 0 &&
            //     IsModelAnimationValid(model.model, model.animations[model.currentAnimation])
            // ) {
            //     m_information.animationTimer += GetFrameTime();
            //     while (m_information.animationTimer >= 1.0f / 60.0f) {
            //         m_information.animationTimer -= 1.0f / 60.0f;
            //         m_information.currentFrame =
            //                 (m_information.currentFrame + 1) % model.animations[model.currentAnimation].frameCount;
            //     }
            //     UpdateModelAnimation(
            //         model.model,
            //         model.animations[model.currentAnimation],
            //         m_information.currentFrame
            //     );
            // }

            BeginDrawing();
            ClearBackground(SKYBLUE);
            BeginMode3D(m_camera);

            DrawGround();

            // for (std::size_t x = 0; x < matrix.width; ++x) {
            //     for (std::size_t y = 0; y < matrix.height; ++y) {
            //         const Vector3 position{
            //             static_cast<float>(x) * zioop::CELL_SIZE, 0.0f,
            //             static_cast<float>(y) * zioop::CELL_SIZE
            //         };
            //         DrawModel(
            //             m_groundModel,
            //             position,
            //             1.0f,
            //             WHITE
            //         );
            //         DrawCubeWires(position, zioop::CELL_SIZE, 0.05f, zioop::CELL_SIZE, DARKGRAY);
            //     }
            // }
            DrawAgents();
            DrawResources();

            EndMode3D();

            // for (const auto &[id, agent]: matrix.agents) {
            //     auto position = agent.position;
            //     position.y += 0.55f;
            //     // Ne pas projeter une etiquette situee derriere la camera.
            //     const float depth = (position.x - m_camera.position.x) * (m_camera.target.x - m_camera.position.x) +
            //                         (position.y - m_camera.position.y) * (m_camera.target.y - m_camera.position.y) +
            //                         (position.z - m_camera.position.z) * (m_camera.target.z - m_camera.position.z);
            //     if (depth > 0.0f) {
            //         const auto screen = GetWorldToScreen(position, m_camera);
            //         const std::string label = "#" + std::to_string(id);
            //         DrawText(label.c_str(), static_cast<int>(screen.x) - MeasureText(label.c_str(), 14) / 2,
            //                  static_cast<int>(screen.y), 14, DARKBLUE);
            //     }
            // }
            // DrawRectangle(5, 5, 570, 115, Fade(RAYWHITE, 0.9f));
            // DrawFPS(10, 10);
            // DrawText(TextFormat("AGENTS: %zu | MATRIX: %zu x %zu", matrix.agents.size(), matrix.width, matrix.height),
            //          10, 35, 20, DARKGRAY);
            // DrawText(matrixStatus.c_str(), 10, 60, 18, connected ? DARKGREEN : MAROON);
            // if (hasSnapshot) {
            //     const auto age = std::chrono::duration<double>(now - lastUpdate).count();
            //     DrawText(TextFormat("Last update: %.1f s | R: recenter | Esc: quit", age), 10, 85, 18, DARKGRAY);
            // }
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

        UnloadModel(m_resourceModel.model);
        UnloadModel(m_groundModel);
        UnloadTexture(m_groundTexture);

        CloseWindow();
    }
}
