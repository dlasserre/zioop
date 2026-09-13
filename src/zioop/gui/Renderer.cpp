#include "zioop/gui/Renderer.hpp"

#include <iostream>
#include <string>

namespace gui {
    using Clock = std::chrono::steady_clock;

    static void centerCamera(Camera3D& camera, const gui::MatrixConnection::MatrixSnapshot& matrix) {
        const float span = static_cast<float>(std::max(matrix.width, matrix.height)) * zioop::CELL_SIZE;
        camera.target = {
            static_cast<float>(matrix.width - 1) * zioop::CELL_SIZE / 2.0f, 0.0f,
            static_cast<float>(matrix.height - 1) * zioop::CELL_SIZE / 2.0f
        };
        const float distance = std::max(span, 4.0f);
        camera.position = {camera.target.x + distance * 0.85f, distance,
                           camera.target.z + distance * 0.85f};
    }

    Renderer::Renderer(
        const int width,
        const int height,
        std::string title
    ) : m_width(width),
        m_height(height),
        m_title(std::move(title))
    {
        std::cout << "Creating renderer width=" << width << " height=" << height << std::endl;
        InitWindow(m_width, m_height, m_title.c_str());
        SetTargetFPS(60);
        AgentVisual& agentVisual = m_agentVisual;

        std::string asset = std::string(GetApplicationDirectory()) + "assets/Alien.gltf";
        if (!FileExists(asset.c_str())) {
            asset = "assets/Alien.gltf";
        }
        // Model model{};

        ModelAnimation* animations = nullptr;
        int animationCount = 0;
        if (FileExists(asset.c_str())) {
            agentVisual.model = LoadModel(asset.c_str());
            if (IsModelValid(agentVisual.model)) {
                agentVisual.animations =
                    LoadModelAnimations(
                        asset.c_str(),
                        &agentVisual.animationCount
                    );
            }
        }

        if (const bool modelReady = IsModelValid(agentVisual.model)) {
            const auto bounds = GetModelBoundingBox(agentVisual.model);
            const float size = std::max({bounds.max.x - bounds.min.x,
                                         bounds.max.y - bounds.min.y,
                                         bounds.max.z - bounds.min.z});
            if (size > 0.0f) {
                agentVisual.scale = 0.45f / size;
            }
            agentVisual.baseY = -bounds.min.y * agentVisual.scale;
            agentVisual.modelReady = modelReady;
        }

        // Par defaut l'animation idle.
        agentVisual.currentAnimation = animationCount > 5 ? 5 : 0;
        DisableCursor();
    }

    void Renderer::drawWorld() {
        bool hasSnapshot = false;
        bool connected = false;
        std::string matrixStatus = "GET_MATRIX: waiting for server...";
        auto nextRequest = Clock::now();
        auto lastUpdate = Clock::now();
        auto& model = m_agentVisual;

        while (!WindowShouldClose()) {
            const auto now = Clock::now();
            auto& matrixRequest = m_matrixRequest;
            auto& matrix = m_matrix;

            if (matrixRequest.valid() &&
                matrixRequest.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                try {
                    auto snapshot = matrixRequest.get();
                    const bool resized = !hasSnapshot || matrix.width != snapshot.width ||
                                         matrix.height != snapshot.height;
                    // Remplacer l'etat complet retire aussi les agents deconnectes.
                    matrix = std::move(snapshot);
                    if (resized) {
                        centerCamera(m_camera, matrix);
                    }
                    hasSnapshot = true;
                    connected = true;
                    lastUpdate = now;
                    matrixStatus = "GET_MATRIX: connected (250 ms)";
                } catch (const std::exception &error) {
                    connected = false;
                    matrixStatus = "GET_MATRIX: error - retrying (see console)";
                    nextRequest = now + std::chrono::seconds(2);
                    std::cerr << error.what() << '\n';
                }
            }
            auto& MatrixConnection = m_connection;
            if (!matrixRequest.valid() && now >= nextRequest) {
                matrixRequest = std::async(std::launch::async, [&MatrixConnection] {
                    return MatrixConnection.request();
                });
                nextRequest = now + zioop::REFRESH_INTERVAL;
            }

            if (hasSnapshot &&IsKeyPressed(KEY_R)) {
                centerCamera(m_camera, matrix);
            }
            UpdateCamera(&m_camera, CAMERA_FREE);

            // Une mise a jour de l'animation par image, quel que soit le nombre d'agents.
            if (
                model.animations && model.animationCount > 0 &&
                model.animations[model.currentAnimation].frameCount > 0 &&
                IsModelAnimationValid(model.model, model.animations[model.currentAnimation])
            ) {
                m_information.animationTimer += GetFrameTime();
                while (m_information.animationTimer >= 1.0f / 60.0f) {
                    m_information.animationTimer -= 1.0f / 60.0f;
                    m_information.currentFrame = (m_information.currentFrame + 1) % model.animations[model.currentAnimation].frameCount;
                }
                UpdateModelAnimation(
                    model.model,
                    model.animations[model.currentAnimation],
                    m_information.currentFrame
                );
            }

            BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(m_camera);

            for (std::size_t x = 0; x < matrix.width; ++x) {
                for (std::size_t y = 0; y < matrix.height; ++y) {
                    const Vector3 position{
                        static_cast<float>(x) * zioop::CELL_SIZE, 0.0f,
                        static_cast<float>(y) * zioop::CELL_SIZE
                    };
                    DrawCube(position, zioop::CELL_SIZE, 0.05f, zioop::CELL_SIZE, LIGHTGRAY);
                    DrawCubeWires(position, zioop::CELL_SIZE, 0.05f, zioop::CELL_SIZE, DARKGRAY);
                }
            }

            for (const auto &[id, agent]: matrix.agents) {
                const Color color = ColorFromHSV(static_cast<float>((id % 12) * 30), 0.7f, 0.8f);
                DrawCylinder(agent.position, 0.19f, 0.19f, 0.015f, 16, color);
                if (model.modelReady) {
                    auto position = agent.position;
                    position.y += model.baseY;
                    DrawModel(model.model, position, model.scale, WHITE);
                } else {
                    auto position = agent.position;
                    position.y += 0.2f;
                    DrawCube(position, 0.25f, 0.4f, 0.25f, color);
                }
            }
            EndMode3D();

            for (const auto &[id, agent]: matrix.agents) {
                auto position = agent.position;
                position.y += 0.55f;
                // Ne pas projeter une etiquette situee derriere la camera.
                const float depth = (position.x - m_camera.position.x) * (m_camera.target.x - m_camera.position.x) +
                                    (position.y - m_camera.position.y) * (m_camera.target.y - m_camera.position.y) +
                                    (position.z - m_camera.position.z) * (m_camera.target.z - m_camera.position.z);
                if (depth > 0.0f) {
                    const auto screen = GetWorldToScreen(position, m_camera);
                    const std::string label = "#" + std::to_string(id);
                    DrawText(label.c_str(), static_cast<int>(screen.x) - MeasureText(label.c_str(), 14) / 2,
                             static_cast<int>(screen.y), 14, DARKBLUE);
                }
            }
            DrawRectangle(5, 5, 570, 115, Fade(RAYWHITE, 0.9f));
            DrawFPS(10, 10);
            DrawText(TextFormat("AGENTS: %zu | MATRIX: %zu x %zu", matrix.agents.size(), matrix.width, matrix.height),
                     10, 35, 20, DARKGRAY);
            DrawText(matrixStatus.c_str(), 10, 60, 18, connected ? DARKGREEN : MAROON);
            if (hasSnapshot) {
                const auto age = std::chrono::duration<double>(now - lastUpdate).count();
                DrawText(TextFormat("Last update: %.1f s | R: recenter | Esc: quit", age), 10, 85, 18, DARKGRAY);
            }
            EndDrawing();
        }
    }

    Renderer::~Renderer() {
        EnableCursor();
        if (m_agentVisual.animations) {
            UnloadModelAnimations(m_agentVisual.animations, m_agentVisual.animationCount);
        }
        if (m_agentVisual.model.meshes || m_agentVisual.model.materials) {
            UnloadModel(m_agentVisual.model);
        }
        CloseWindow();
    }
}
