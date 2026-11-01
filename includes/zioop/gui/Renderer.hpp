#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX

#include "MatrixConnection.hpp"
#include "Terrain.hpp"
#include "DayCycle.hpp"
#include "Lighting.hpp"
#include "Typography.hpp"

#include <deque>
#include <future>
#include <memory>
#include <string>
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
            float               height{0.4f};
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
            float ritualTime{0.0f};
            float burst{0.0f};

            std::size_t lookCount{0};
            float lookTime{-1.0f};
            Vector3 lookDirection{0.0f, 0.0f, -1.0f};
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

        void DrawAgents() const;

        void DrawAgentInformation() const;

        void DrawEnvironmentInformation() const;

        void collectAgentLights(std::vector<PointLight> &lights, float night) const;

        void renderScene(float deltaTime);

        void DrawRitualEffects() const;

        void DrawLookBeams() const;

        void DrawLevelAuras() const;

        void updateCamera(float deltaTime);

        std::string m_moveKeys{"WASD"};

        [[nodiscard]] Vector3 eyePosition(const AgentMotion &motion) const;

        Texture2D m_glow{};

        void DrawSpeechBubble(const std::string &message, Vector2 tip, Color border) const;

        float DrawKeyHint(const std::string &key, const std::string &label, Vector2 position) const;

        Typography m_typography{};

        struct ChatEntry {
            std::size_t agentId{};
            std::size_t x{};
            std::size_t y{};
            std::string time{};
            std::string message{};
        };

        void recordMessages(const MatrixConnection::MatrixSnapshot &snapshot);

        void DrawChat() const;

        std::deque<ChatEntry> m_chat{};

        bool m_showChat{true};

        void DrawResources() const;

        int m_width{1}, m_height{1};

        std::unique_ptr<Lighting> m_lighting;

        DayCycle m_dayCycle{};

        float m_elapsed{0.0f};
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

        struct ResourceModel {
            ModelRender resource;
            float scale{0.3f};
            float zOffset{.095f};
            float minY{0.0f};
        };

        ModelRender m_agentModel{};
        std::unordered_map<int, ResourceModel> m_resourceModels{};
        std::unordered_map<int, Texture2D> m_inventoryTextures{};

        PanelInformation m_panelInformation{};

        void updateAgentMotions(ModelRender& model, float deltaTime);

        Terrain m_terrain;

        Texture2D m_groundTexture{};

        std::unordered_map<std::size_t, AgentMotion> m_agentMotions;

        std::vector<Corpse> m_corpses;
    };
}
