#pragma once

#include <cstdint>
#include <atomic>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <string_view>

#include "zioop/core/Matrix.hpp"
#include "zioop/network/Client.hpp"

namespace ai {
    class Bot {
    public:
        const std::array<core::Direction, 4> directions{
            core::Direction::NORTH,
            core::Direction::SOUTH,
            core::Direction::EAST,
            core::Direction::WEST
        };

        struct Decision {
            std::size_t id{0};

            network::Protocol::MessageType command;
            nlohmann::json parameters;

            std::optional<network::Protocol::Response> response;

            std::string reason;
        };

        Bot(std::string ip, std::uint16_t port);

        virtual ~Bot();

        void run(
            const std::atomic_bool &stopRequested
        );

    protected:
        // Properties
        std::size_t m_agentId{0};

        // Methods
        [[nodiscard]]
        virtual std::string_view getName() const = 0;

        virtual void think(
            const network::Protocol::Response& response
        ) = 0;

        virtual void sendCommand(
            network::Protocol::MessageType messageType,
            const std::string &payload
        ) const final;

        [[nodiscard]]
        std::size_t getAgentId() const {
            return m_agentId;
        }

        void addHistory(Decision decision);
        [[nodiscard]]
        std::optional<Decision> lastDecision() const;

    private:

        SOCKET m_socket{INVALID_SOCKET};

        std::string m_ip;
        std::uint16_t m_port;
        std::queue<network::Protocol::Response> m_queue;
        std::mutex m_mutex{};
        bool m_thinkStopped{false};
        std::condition_variable m_responseAvailable;
        std::deque<Decision> m_history{};

        void connect() const;
        void disconnect();
        void enqueue(network::Protocol::Response response);
        void receiveExactly(char *buffer, int size) const;
        [[nodiscard]] network::Protocol::Response receiveResponse() const;
    };
}
