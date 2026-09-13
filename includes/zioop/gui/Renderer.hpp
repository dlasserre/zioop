#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX

#include "MatrixConnection.hpp"

#include <future>

namespace zioop {
    inline constexpr float CELL_SIZE = 1.0f;

    inline constexpr auto REFRESH_INTERVAL = std::chrono::milliseconds(250);
}

namespace gui {
    class Renderer {
    public:

        struct Information {
            int currentFrame{0};
            float animationTimer{0.0f};
        };

        struct AgentVisual {
            Model               model{};
            ModelAnimation*     animations{};
            int                 animationCount{};
            float               scale{0.1f};
            float               baseY{0.0f};
            int                 currentAnimation{0};
            bool                modelReady{false};
        };

        Renderer(
            int         width,
            int         height,
            std::string title
        );

        Renderer(Renderer &) = delete;

        void operator=(Renderer &) = delete;

        [[nodiscard]] std::shared_ptr<MatrixConnection *> getMatrixConnection() {
            return std::make_shared<MatrixConnection *>(&m_connection);
        }

        void drawWorld();

        [[nodiscard]] AgentVisual &getModel() {
            return m_agentVisual;
        }

        ~Renderer();

    private:
        void updateNetwork();

        void updateCamera();

        void updateAnimations();

        void drawUI();

        int m_width{1}, m_height{1};
        std::string m_title{};

        Camera3D m_camera{
            .position = {10.0f, 10.0f, 10.0f},
            .up = {0.0f, 1.0f, 0.0f},
            .fovy = 60.0f,
            .projection = CAMERA_PERSPECTIVE
        };

        Information m_information{};

        AgentVisual m_agentVisual{};

        MatrixConnection m_connection{};

        MatrixConnection::MatrixSnapshot m_matrix;

        std::future<MatrixConnection::MatrixSnapshot> m_matrixRequest;
    };
}
