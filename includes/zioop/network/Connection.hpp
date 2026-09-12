#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <cstddef>
#include <functional>
#include <winsock2.h>

#include "zioop/network/Protocol.hpp"

namespace network {
    class Connection {
    public:
        using FrameHandler = std::function<
            void(Connection &, const Protocol::Frame &)
        >;

        using DisconnectHandler = std::function<
            void(Connection&)
        >;

        Connection(
            SOCKET socket,
            std::string ip,
            std::uint16_t port,
            FrameHandler frameHandler,
            DisconnectHandler disconnectHandler
        );

        ~Connection();

        void send(const std::vector<std::byte>& data) const;

        void start();

        void run();

        void requestStop();

        void join();

    private:
        SOCKET m_socket{INVALID_SOCKET};

        std::string m_ip;
        std::uint16_t m_port{};
        std::thread m_thread;

        std::vector<std::byte> m_receiveBuffer;

        std::atomic_bool m_running{false};

        FrameHandler m_frameHandler;
        DisconnectHandler m_disconnectHandler;
    };
}
