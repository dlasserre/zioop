#pragma once

#include "EventDispatcher.hpp"
#include "zioop/core/Game.hpp"
#include "zioop/network/Connection.hpp"
#include "zioop/network/Protocol.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <queue>
#include <winsock2.h>

namespace network {
    constexpr int MAX_MESSAGE_SIZE{255};

    struct Request {
        core::AgentId agentId{0};
        core::ClientId clientId{0};
        nlohmann::json body;
    };

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

        void Start();

        void stop();

        void tick();

    private:
        using Handler = void (Server::*)(
            Connection &,
            const Request &
        );

        void processDisconnections();

        void handleFrame(
            Connection &connection,
            const Protocol::Frame &frame
        );

        void handleMessage(
            Connection &connection,
            const Protocol::Message &message
        );

        void broadcast(
            Connection &connection,
            const Request &request
        );

        void hello(
            Connection &connection,
            const Request &request
        );

        void move(
            Connection &connection,
            const Request &request
        );

        void getMatrix(
            Connection &connection,
            const Request &request
        );

        void lookingAt(
            Connection &connection,
            const Request &request
        );

        void take(
            Connection &connection,
            const Request &request
        );

        void give(
            Connection &connection,
            const Request &request
        );

        void disconnect(
            Connection &connection,
            const Request &message
        );

        void inventory(
            Connection &connection,
            const Request &request
        );

        void ritual(
            Connection &connection,
            const Request &request
        );

        void attack(
            Connection &connection,
            const Request &request
        );

        void invocation(
            Connection &connection,
            const Request &request
        );

        void heal(
            Connection &connection,
            const Request &request
        );

        std::mutex m_stateMutex;

        core::Game m_game;
        std::atomic_bool m_running{false};

        std::string m_address;
        std::uint16_t m_port{};

        std::mutex m_disconnectionsMutex;
        std::queue<Connection *> m_disconnections;

        SOCKET m_socket{INVALID_SOCKET};

        std::size_t m_nextClientId{0};
        std::vector<std::shared_ptr<Connection> > m_connections;

        EventDispatcher m_dispatcher;
    };
}
