#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <queue>

#include <winsock2.h>

#include "zioop/core/Game.hpp"
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

        void tick();

    private:
        using Handler = void (Server::*)(
            Connection &,
            const Protocol::Message &
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

        void lookingAt(
            Connection &connection,
            const Protocol::Message &message
        );

        void take(
            Connection &connection,
            const Protocol::Message &message
        );

        void disconnect(
            Connection &connection,
            const Protocol::Message &message
        );


        void inventory(
            Connection &connection,
            const Protocol::Message &message
        );

        void ritual(
            Connection &connection,
            const Protocol::Message &message
        );

        void attack(
            Connection &connection,
            const Protocol::Message &message
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
    };
}
