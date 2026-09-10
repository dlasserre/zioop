#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <winsock2.h>

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

        void Start();

        void stop();

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

        void getMatrix(
            Connection &connection,
            const Protocol::Message &message
        );

        std::optional<core::Position> addAgent();

        core::Matrix m_matrix;
        std::vector<core::Agent> m_agents;

        std::atomic_bool m_running{false};

        std::string m_address;
        std::uint16_t m_port{};

        SOCKET m_socket{INVALID_SOCKET};

        std::vector<std::unique_ptr<Connection> > m_connections;
    };
}
