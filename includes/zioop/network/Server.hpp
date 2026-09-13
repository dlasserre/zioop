#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>

#include <winsock2.h>

#include "zioop/network/ClientSession.hpp"
#include "zioop/core/Matrix.hpp"
#include "zioop/network/Connection.hpp"
#include "zioop/network/Protocol.hpp"

namespace network {
    class Server {
    public:
        Server(
            std::string address,
            std::uint16_t port,
            core::Matrix matrix
        );

        ~Server();

        Server(const Server &) = delete;

        Server &operator=(const Server &) = delete;

        Server(Server &&) = delete;

        Server &operator=(Server &&) = delete;

        void processDisconnections();

        void Start();

        void stop();

    private:
        using Handler = void (Server::*)(
            Connection &,
            const Protocol::Message &
        );

        ClientSession* getClientSession(
            Connection &connection
        );

        void handleFrame(
            Connection &connection,
            const Protocol::Frame &frame
        );

        void handleMessage(
            Connection &connection,
            const Protocol::Message &message
        );

        void hello(
            Connection &connection,
            const Protocol::Message &message
        );

        void move(
            Connection &connection,
            const Protocol::Message &message
        );

        void getMatrix(
            Connection &connection,
            const Protocol::Message &message
        );

        std::unordered_map<
            Connection *,
            std::unique_ptr<ClientSession>
        > m_sessions;

        std::mutex m_stateMutex;

        struct AgentPlacement {
            std::size_t agentId;
            core::Position position;
        };

        std::optional<AgentPlacement> addAgent(
            ClientSession& session,
            core::ANGLE angle
        );

        core::Matrix m_matrix;

        std::atomic_bool m_running{false};

        std::string m_address;
        std::uint16_t m_port{};

        std::mutex m_disconnectionsMutex;
        std::queue<Connection*> m_disconnections;

        SOCKET m_socket{INVALID_SOCKET};

        std::vector<std::unique_ptr<Connection>> m_connections;
    };
}
