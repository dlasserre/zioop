#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

#include "Connection.hpp"
#include "Protocol.hpp"

namespace network {
    class Client {
    public:
        Client(std::string ip, std::uint16_t port);

        Client(Client const &) = delete;
        Client &operator=(Client const &) = delete;

        Client(Client &&) = delete;
        Client &operator=(Client &&) = delete;

        void connect();

        void send(
            Protocol::MessageType type,
            const std::string &payload
        );

        [[nodiscard]] std::size_t getId() const {
            if (!m_agentId.has_value()) {
                throw std::logic_error(
                    "Server has not assigned an ID yet"
                );
            }

            return *m_agentId;
        }

        Protocol::Response waitForResponse();

        void disconnect();

        ~Client();

    private:
        std::queue<Protocol::Response> m_responses;
        mutable std::mutex m_mutex;
        std::condition_variable m_responseAvailable;
        bool m_connected{false};

        std::string m_ip;
        std::uint16_t m_port;

        std::unique_ptr<Connection> m_connection;

        std::optional<std::size_t> m_agentId; // Need to generate id server side.
    };
}
