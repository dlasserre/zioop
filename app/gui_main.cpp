#include <raylib.h>

#include <string>

#include "zioop/core/Matrix.hpp"
#include "zioop/gui/Agent.hpp"


int main() {
    constexpr int WINDOW_WIDTH = 1280;
    constexpr int WINDOW_HEIGHT = 720;
    constexpr float CELL_SIZE = 1.0f;

    InitWindow(WINDOW_WIDTH,WINDOW_HEIGHT,"Zioop");

    SetTargetFPS(60);

    core::Matrix matrix(20, 20);

    gui::Agent player(
        "assets/Alien.gltf"
    );

    matrix.addAgent(player.getAgent(),core::TOP_LEFT);

    Camera3D camera{};
    camera.position = {10.0f,10.0f,10.0f};
    camera.target = {0.0f,0.0f,0.0f};
    camera.up = {0.0f,1.0f,0.0f};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    DisableCursor();

    unsigned int currentAnimation = 5 ; // Idle
    int currentFrame = 0;
    float animationTimer = 0.0f;
    constexpr float ANIMATION_FPS = 60.0f;

    while (!WindowShouldClose()) {
        UpdateCamera(&camera,CAMERA_FREE);
        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);

        const auto &data =
                matrix.getMatrix();

        for (std::size_t x = 0; x < data.size(); ++x) {
            for (std::size_t y = 0; y < data[x].size(); ++y) {
                const auto &cell =
                        data[x][y];

                const Vector3 position{
                    static_cast<float>(x) * CELL_SIZE,
                    0.0f,
                    static_cast<float>(y) * CELL_SIZE
                };

                DrawCube(
                    position,
                    CELL_SIZE,
                    0.05f,
                    CELL_SIZE,
                    LIGHTGRAY
                );

                DrawCubeWires(
                    position,
                    CELL_SIZE,
                    0.05f,
                    CELL_SIZE,
                    DARKGRAY
                );

                for (const auto &entry: cell.agents) {
                    const core::Agent *agent = entry.second;
                    const Vector3 agentPosition{
                        position.x,
                        0.05f,
                        position.z
                    };

                    if (agent->getId() == player.getAgent().getId()) {


                        animationTimer += GetFrameTime();

                        if (animationTimer >= 1.0f / ANIMATION_FPS) {
                            animationTimer = 0.0f;

                            ++currentFrame;

                            const auto* animations = player.getAnimations();

                            if (currentFrame >= animations[currentAnimation].frameCount) {
                                currentFrame = 0;
                            }

                            UpdateModelAnimation(
                                player.getModel(),
                                animations[currentAnimation],
                                currentFrame
                            );
                        }

                        DrawModel(
                            player.getModel(),
                            agentPosition,
                            0.1f,
                            WHITE
                        );
                    }
                }
            }
        }

        EndMode3D();
        DrawFPS(10,10);
        DrawText(
            TextFormat("AGENTS: %zu",matrix.getTotalAgents()),
            10,
            35,
            20,
            DARKGRAY
        );
        EndDrawing();
    }

    EnableCursor();
    CloseWindow();

    return 0;
}
