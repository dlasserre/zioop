#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX

#include "MatrixConnection.hpp"

#include <future>
#include <vector>

namespace zioop {
    inline constexpr float CELL_SIZE = 1.0f;

    inline constexpr auto REFRESH_INTERVAL = std::chrono::milliseconds(250);
}

namespace gui {
    using Clock = std::chrono::steady_clock;

    class Renderer {
    public:

        struct Information {
            int currentFrame{0};
            float animationTimer{0.0f};
        };

        struct  PanelInformation {
            bool connected = false;
            std::chrono::time_point<Clock> nextRequest = Clock::now();
            std::chrono::time_point<Clock> lastUpdate = Clock::now();
            std::string status{};
        };

        struct ModelRender {
            Model               model{};
            ModelAnimation*     animations{};
            int                 animationCount{};
            float               scale{0.1f};
            float               baseY{0.0f};
            bool                modelReady{false};
            int                 walkAnimation{-1};
            int                 idleAnimation{-1};
            int                 attackAnimation{-1};
            int                 takeAnimation{-1};
            int                 ritualAnimation{-1};
            int                 levelUpAnimation{-1};
            int                 deathAnimation{-1};
        };

        struct AgentMotion {
            Vector3 from{};
            Vector3 current{};
            Vector3 target{};

            float elapsed{0.0f};
            float rotation{0.0f};

            int animation{-1};
            int frame{0};
            float animationTimer{0.0f};

            // Action jouée une seule fois, après le déplacement éventuel.
            std::size_t actionCount{0};
            int actionAnimation{-1};

            std::size_t level{0};
            bool ritual{false};
        };

        // Agent tué : il joue Death une fois, puis reste au sol.
        struct Corpse {
            Vector3 position{};
            float rotation{0.0f};
            int frame{0};
            float animationTimer{0.0f};
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

        ~Renderer();

    private:
        void updateMatrix(bool& hasSnapshot, Clock::time_point& nextReques);

        void DrawAgents();

        void DrawResources() const;

        void DrawGround();

        int m_width{1}, m_height{1};
        std::string m_title{};

        Camera3D m_camera{
            .position = {10.0f, 10.0f, 10.0f},
            .up = {0.0f, 1.0f, 0.0f},
            .fovy = 60.0f,
            .projection = CAMERA_PERSPECTIVE
        };

        MatrixConnection m_connection{};

        MatrixConnection::MatrixSnapshot m_matrix;

        std::future<MatrixConnection::MatrixSnapshot> m_matrixRequest;

        ModelRender m_agentModel{};
        ModelRender m_resourceModel{};

        PanelInformation m_panelInformation{};

        void updateAgentMotions(ModelRender& model, float deltaTime);

        ::Model m_groundModel{};

        Texture2D m_groundTexture{};

        std::unordered_map<std::size_t, AgentMotion> m_agentMotions;

        std::vector<Corpse> m_corpses;
    };
}
