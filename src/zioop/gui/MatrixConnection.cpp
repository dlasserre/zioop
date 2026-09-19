#include "zioop/gui/MatrixConnection.hpp"

#include <iostream>

#include "zioop/network/Protocol.hpp"

#include <nlohmann/json.hpp>

#include "zioop/gui/Renderer.hpp"

namespace gui {
    static std::size_t readUnsigned(const nlohmann::json &value) {
        if (!value.is_number_integer() || value < 0) {
            throw std::runtime_error("Expected a non-negative integer in GET_MATRIX");
        }
        return value.get<std::size_t>();
    }

    static MatrixConnection::MatrixSnapshot parseMatrix(const nlohmann::json &response) {
        const auto &matrix = response.at("matrix");
        MatrixConnection::MatrixSnapshot snapshot;
        snapshot.width = readUnsigned(matrix.at("xSize"));
        snapshot.height = readUnsigned(matrix.at("ySize"));
        // Limiter le nombre de cases a dessiner dans cette premiere version.
        if (snapshot.width == 0 || snapshot.height == 0 ||
            snapshot.width > 65536 / snapshot.height) {
            throw std::runtime_error("Invalid or too large matrix dimensions");
        }

        const auto &agents = matrix.at("data").at("agents");
        if (!agents.is_array()) {
            throw std::runtime_error("GET_MATRIX data.agents must be an array");
        }

        for (const auto &entry: agents) {
            const auto id = readUnsigned(entry.at("agentId"));
            const auto &position = entry.at("position");
            MatrixConnection::AgentView agent;
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
        for (auto &[id, agent]: snapshot.agents) {
            auto &slot = occupants[{agent.x, agent.y}];
            if (slot >= CELL_OFFSETS.size()) {
                throw std::runtime_error("More than four agents in one cell");
            }
            const auto offset = CELL_OFFSETS[slot++];
            agent.position = {
                (static_cast<float>(agent.x) + offset.x) * zioop::CELL_SIZE,
                0.05f,
                (static_cast<float>(agent.y) + offset.y) * zioop::CELL_SIZE
            };
        }

        std::map<std::pair<std::size_t, std::size_t>, std::size_t> resourceSlots;

        const auto& resources = matrix.at("data").at("resources");

        if (!resources.is_array()) {
            throw std::runtime_error("Resources must be an array");
        }

        for (const auto& entry : resources) {
            const auto& position = entry.at("position");
            const auto x = readUnsigned(position.at("x"));
            const auto y = readUnsigned(position.at("y"));

            if (x >= snapshot.width || y >= snapshot.height) {
                throw std::runtime_error("Resource outside the matrix");
            }

            // Décaler les fruits d'une même case pour les distinguer.
            const auto slot = resourceSlots[{x, y}]++;
            const float offsetX =
                (static_cast<float>(slot % 3) - 1.0f) * 0.22f;
            const float offsetZ =
                static_cast<float>(slot / 3) * 0.22f;

            snapshot.resources.push_back({
                .type = entry.at("type").get<int>(),
                .rotting = entry.at("rotting").get<int>(),
                .position = {
                    (static_cast<float>(x) + offsetX) * zioop::CELL_SIZE,
                    0.12f,
                    (static_cast<float>(y) + offsetZ) * zioop::CELL_SIZE
                }
            });
        }


        return snapshot;
    }

    MatrixConnection::MatrixConnection() {
        WSADATA wsaData{};
        const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup() failed: " + std::to_string(result));
        }
    }

    MatrixConnection::MatrixSnapshot MatrixConnection::request() {
        using Protocol = network::Protocol;
        try {
            if (m_socket == INVALID_SOCKET) {
                open();
            }

            const auto message = Protocol::encode(Protocol::MessageType::GET_MATRIX, "{}");
            std::size_t totalSent = 0;
            while (totalSent < message.size()) {
                const int sent = send(m_socket,
                                      reinterpret_cast<const char *>(message.data() + totalSent),
                                      static_cast<int>(message.size() - totalSent), 0);
                if (sent <= 0) {
                    throw std::runtime_error("send() failed: " + std::to_string(WSAGetLastError()));
                }
                totalSent += static_cast<std::size_t>(sent);
            }

            std::uint16_t networkType{};
            std::uint32_t networkLength{};
            receiveExactly(reinterpret_cast<char *>(&networkType), sizeof(networkType));
            receiveExactly(reinterpret_cast<char *>(&networkLength), sizeof(networkLength));
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
            // std::cout << "GET_MATRIX: " << payload << std::endl;
            return parseMatrix(nlohmann::json::parse(payload));
        } catch (...) {
            close();
            throw;
        }
    }

    void MatrixConnection::open() {
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) {
            throw std::runtime_error("socket() failed: " + std::to_string(WSAGetLastError()));
        }
        const DWORD timeoutMs = 2000;
        if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs)) == SOCKET_ERROR ||
            setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO,
                       reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs)) == SOCKET_ERROR) {
            throw std::runtime_error("setsockopt() failed: " + std::to_string(WSAGetLastError()));
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(9090);
        inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        if (::connect(m_socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR) {
            throw std::runtime_error("connect() failed: " + std::to_string(WSAGetLastError()));
        }
    }

    void MatrixConnection::close() {
        if (m_socket != INVALID_SOCKET) {
            shutdown(m_socket, SD_SEND);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    void MatrixConnection::receiveExactly(char *buffer, int size) const {
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

    MatrixConnection::~MatrixConnection() {
        close();
        WSACleanup();
    }
}
