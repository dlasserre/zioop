#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <raylib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

#include "zioop/network/Protocol.hpp"

namespace {
    using Clock = std::chrono::steady_clock;
    constexpr float CELL_SIZE = 1.0f;
    constexpr auto REFRESH_INTERVAL = std::chrono::milliseconds(250);
    constexpr std::array<Vector2, 4> CELL_OFFSETS{{
        {-0.25f, -0.25f}, {0.25f, -0.25f},
        {-0.25f, 0.25f}, {0.25f, 0.25f}
    }};

    // Une copie pour l'affichage : les IDs et les positions viennent du serveur.
    struct AgentView {
        std::size_t x{};
        std::size_t y{};
        Vector3 position{};
    };

    struct MatrixSnapshot {
        std::size_t width{};
        std::size_t height{};
        std::map<std::size_t, AgentView> agents;
    };

    std::size_t readUnsigned(const nlohmann::json& value) {
        if (!value.is_number_integer() || value < 0) {
            throw std::runtime_error("Expected a non-negative integer in GET_MATRIX");
        }
        return value.get<std::size_t>();
    }

    MatrixSnapshot parseMatrix(const nlohmann::json& response) {
        const auto& matrix = response.at("matrix");
        MatrixSnapshot snapshot;
        snapshot.width = readUnsigned(matrix.at("xSize"));
        snapshot.height = readUnsigned(matrix.at("ySize"));
        // Limiter le nombre de cases a dessiner dans cette premiere version.
        if (snapshot.width == 0 || snapshot.height == 0 ||
            snapshot.width > 65536 / snapshot.height) {
            throw std::runtime_error("Invalid or too large matrix dimensions");
        }

        const auto& agents = matrix.at("data").at("agents");
        if (!agents.is_array()) {
            throw std::runtime_error("GET_MATRIX data.agents must be an array");
        }

        for (const auto& entry : agents) {
            const auto id = readUnsigned(entry.at("agentId"));
            const auto& position = entry.at("position");
            AgentView agent;
            agent.x = readUnsigned(position.at("x"));
            agent.y = readUnsigned(position.at("y"));
            if (agent.x >= snapshot.width || agent.y >= snapshot.height) {
                throw std::runtime_error("Agent position outside the matrix");
            }
            if (!snapshot.agents.emplace(id, agent).second) {
                throw std::runtime_error("Duplicate agent ID in GET_MATRIX");
            }
        }

        // L'ordre des IDs reste stable meme si le JSON arrive dans un autre ordre.
        std::map<std::pair<std::size_t, std::size_t>, std::size_t> occupants;
        for (auto& [id, agent] : snapshot.agents) {
            auto& slot = occupants[{agent.x, agent.y}];
            if (slot >= CELL_OFFSETS.size()) {
                throw std::runtime_error("More than four agents in one cell");
            }
            const auto offset = CELL_OFFSETS[slot++];
            agent.position = {
                (static_cast<float>(agent.x) + offset.x) * CELL_SIZE,
                0.05f,
                (static_cast<float>(agent.y) + offset.y) * CELL_SIZE
            };
        }
        return snapshot;
    }

    // Une seule connexion TCP, utilisee uniquement par la requete asynchrone.
    class MatrixConnection {
    public:
        MatrixConnection() {
            WSADATA wsaData{};
            const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                throw std::runtime_error("WSAStartup() failed: " + std::to_string(result));
            }
        }

        MatrixConnection(const MatrixConnection&) = delete;
        MatrixConnection& operator=(const MatrixConnection&) = delete;

        ~MatrixConnection() {
            close();
            WSACleanup();
        }

        MatrixSnapshot request() {
            using Protocol = network::Protocol;
            try {
                if (m_socket == INVALID_SOCKET) {
                    open();
                }

                const auto message = Protocol::encode(Protocol::MessageType::GET_MATRIX, "{}");
                std::size_t totalSent = 0;
                while (totalSent < message.size()) {
                    const int sent = send(m_socket,
                        reinterpret_cast<const char*>(message.data() + totalSent),
                        static_cast<int>(message.size() - totalSent), 0);
                    if (sent <= 0) {
                        throw std::runtime_error("send() failed: " + std::to_string(WSAGetLastError()));
                    }
                    totalSent += static_cast<std::size_t>(sent);
                }

                std::uint16_t networkType{};
                std::uint32_t networkLength{};
                receiveExactly(reinterpret_cast<char*>(&networkType), sizeof(networkType));
                receiveExactly(reinterpret_cast<char*>(&networkLength), sizeof(networkLength));
                const auto type = static_cast<Protocol::ResponseType>(ntohs(networkType));
                const auto length = ntohl(networkLength);
                if (length > 1024 * 1024) {
                    throw std::runtime_error("Response payload too large");
                }

                std::string payload(length, '\0');
                receiveExactly(payload.data(), static_cast<int>(payload.size()));
                if (type == Protocol::ResponseType::KO) {
                    throw std::runtime_error("GET_MATRIX rejected: " + payload);
                }
                if (type != Protocol::ResponseType::OK && type != Protocol::ResponseType::MATRIX_STATE) {
                    throw std::runtime_error("Unexpected response to GET_MATRIX");
                }
                return parseMatrix(nlohmann::json::parse(payload));
            } catch (...) {
                close();
                throw;
            }
        }

    private:
        SOCKET m_socket = INVALID_SOCKET;

        void close() {
            if (m_socket != INVALID_SOCKET) {
                shutdown(m_socket, SD_SEND);
                closesocket(m_socket);
                m_socket = INVALID_SOCKET;
            }
        }

        void open() {
            m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (m_socket == INVALID_SOCKET) {
                throw std::runtime_error("socket() failed: " + std::to_string(WSAGetLastError()));
            }
            const DWORD timeoutMs = 2000;
            if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO,
                           reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs)) == SOCKET_ERROR ||
                setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO,
                           reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs)) == SOCKET_ERROR) {
                throw std::runtime_error("setsockopt() failed: " + std::to_string(WSAGetLastError()));
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(9090);
            inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
            if (::connect(m_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
                throw std::runtime_error("connect() failed: " + std::to_string(WSAGetLastError()));
            }
        }

        void receiveExactly(char* buffer, int size) const {
            int totalReceived = 0;
            while (totalReceived < size) {
                const int received = recv(m_socket, buffer + totalReceived, size - totalReceived, 0);
                if (received == SOCKET_ERROR) {
                    throw std::runtime_error("recv() failed: " + std::to_string(WSAGetLastError()));
                }
                if (received == 0) {
                    throw std::runtime_error("Server disconnected");
                }
                totalReceived += received;
            }
        }
    };

    void centerCamera(Camera3D& camera, const MatrixSnapshot& matrix) {
        const float span = static_cast<float>(std::max(matrix.width, matrix.height)) * CELL_SIZE;
        camera.target = {
            static_cast<float>(matrix.width - 1) * CELL_SIZE / 2.0f, 0.0f,
            static_cast<float>(matrix.height - 1) * CELL_SIZE / 2.0f
        };
        const float distance = std::max(span, 4.0f);
        camera.position = {camera.target.x + distance * 0.85f, distance,
                           camera.target.z + distance * 0.85f};
    }
}

int main() {
    MatrixConnection connection;
    InitWindow(1280, 720, "Zioop");
    SetTargetFPS(60);

    // Le modele est partage par toutes les representations des agents.
    std::string asset = std::string(GetApplicationDirectory()) + "assets/Alien.gltf";
    if (!FileExists(asset.c_str())) {
        asset = "assets/Alien.gltf";
    }
    Model model{};
    ModelAnimation* animations = nullptr;
    int animationCount = 0;
    if (FileExists(asset.c_str())) {
        model = LoadModel(asset.c_str());
        if (IsModelValid(model)) {
            animations = LoadModelAnimations(asset.c_str(), &animationCount);
        }
    }
    const bool modelReady = IsModelValid(model);
    float modelScale = 0.1f;
    float modelBaseY = 0.0f;
    if (modelReady) {
        const auto bounds = GetModelBoundingBox(model);
        const float size = std::max({bounds.max.x - bounds.min.x,
                                     bounds.max.y - bounds.min.y,
                                     bounds.max.z - bounds.min.z});
        if (size > 0.0f) {
            modelScale = 0.45f / size;
        }
        modelBaseY = -bounds.min.y * modelScale;
    }
    const int currentAnimation = animationCount > 5 ? 5 : 0;
    int currentFrame = 0;
    float animationTimer = 0.0f;

    Camera3D camera{};
    camera.position = {10.0f, 10.0f, 10.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    DisableCursor();

    MatrixSnapshot matrix;
    bool hasSnapshot = false;
    bool connected = false;
    std::string matrixStatus = "GET_MATRIX: waiting for server...";
    auto nextRequest = Clock::now();
    auto lastUpdate = Clock::now();
    // Declaree apres connection : elle finit avant la destruction de la socket.
    std::future<MatrixSnapshot> matrixRequest;

    while (!WindowShouldClose()) {
        const auto now = Clock::now();
        if (matrixRequest.valid() &&
            matrixRequest.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            try {
                auto snapshot = matrixRequest.get();
                const bool resized = !hasSnapshot || matrix.width != snapshot.width ||
                                     matrix.height != snapshot.height;
                // Remplacer l'etat complet retire aussi les agents deconnectes.
                matrix = std::move(snapshot);
                if (resized) {
                    centerCamera(camera, matrix);
                }
                hasSnapshot = true;
                connected = true;
                lastUpdate = now;
                matrixStatus = "GET_MATRIX: connected (250 ms)";
            } catch (const std::exception& error) {
                connected = false;
                matrixStatus = "GET_MATRIX: error - retrying (see console)";
                nextRequest = now + std::chrono::seconds(2);
                std::cerr << error.what() << '\n';
            }
        }
        if (!matrixRequest.valid() && now >= nextRequest) {
            matrixRequest = std::async(std::launch::async, [&connection] {
                return connection.request();
            });
            nextRequest = now + REFRESH_INTERVAL;
        }

        if (hasSnapshot && IsKeyPressed(KEY_R)) {
            centerCamera(camera, matrix);
        }
        UpdateCamera(&camera, CAMERA_FREE);

        // Une mise a jour de l'animation par image, quel que soit le nombre d'agents.
        if (animations && animationCount > 0 && animations[currentAnimation].frameCount > 0 &&
            IsModelAnimationValid(model, animations[currentAnimation])) {
            animationTimer += GetFrameTime();
            while (animationTimer >= 1.0f / 60.0f) {
                animationTimer -= 1.0f / 60.0f;
                currentFrame = (currentFrame + 1) % animations[currentAnimation].frameCount;
            }
            UpdateModelAnimation(model, animations[currentAnimation], currentFrame);
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        for (std::size_t x = 0; x < matrix.width; ++x) {
            for (std::size_t y = 0; y < matrix.height; ++y) {
                const Vector3 position{static_cast<float>(x) * CELL_SIZE, 0.0f,
                                       static_cast<float>(y) * CELL_SIZE};
                DrawCube(position, CELL_SIZE, 0.05f, CELL_SIZE, LIGHTGRAY);
                DrawCubeWires(position, CELL_SIZE, 0.05f, CELL_SIZE, DARKGRAY);
            }
        }
        for (const auto& [id, agent] : matrix.agents) {
            const Color color = ColorFromHSV(static_cast<float>((id % 12) * 30), 0.7f, 0.8f);
            DrawCylinder(agent.position, 0.19f, 0.19f, 0.015f, 16, color);
            if (modelReady) {
                auto position = agent.position;
                position.y += modelBaseY;
                DrawModel(model, position, modelScale, WHITE);
            } else {
                auto position = agent.position;
                position.y += 0.2f;
                DrawCube(position, 0.25f, 0.4f, 0.25f, color);
            }
        }
        EndMode3D();

        for (const auto& [id, agent] : matrix.agents) {
            auto position = agent.position;
            position.y += 0.55f;
            // Ne pas projeter une etiquette situee derriere la camera.
            const float depth = (position.x - camera.position.x) * (camera.target.x - camera.position.x) +
                                (position.y - camera.position.y) * (camera.target.y - camera.position.y) +
                                (position.z - camera.position.z) * (camera.target.z - camera.position.z);
            if (depth > 0.0f) {
                const auto screen = GetWorldToScreen(position, camera);
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

    EnableCursor();
    if (animations) {
        UnloadModelAnimations(animations, animationCount);
    }
    if (model.meshes || model.materials) {
        UnloadModel(model);
    }
    CloseWindow();
    return 0;
}
