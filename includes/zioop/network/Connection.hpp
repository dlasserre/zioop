#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
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
            DisconnectHandler disconnectHandler,
            std::optional<std::size_t> clientId = std::nullopt
        );

        ~Connection();

        [[nodiscard]] std::size_t getClientId() const {
            return m_clientId.value();
        }

        void send(const std::vector<std::byte>& data) const;

        void start();

        void run();

        void requestStop();

        void join();

    private:
        const std::optional<std::size_t> m_clientId;
        std::atomic<SOCKET> m_socket{INVALID_SOCKET};

        mutable std::mutex m_sendMutex;

        std::string m_ip;
        std::uint16_t m_port{};
        std::thread m_thread;

        std::vector<std::byte> m_receiveBuffer;

        std::atomic_bool m_running{false};

        FrameHandler m_frameHandler;
        DisconnectHandler m_disconnectHandler;
    };
}
