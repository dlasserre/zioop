#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <optional>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <random>
#include <string_view>
#include <nlohmann/json.hpp>

#include "zioop/core/Types.hpp"
#include "zioop/network/Protocol.hpp"

namespace ai {
    class Bot {
    public:
        Bot(std::string ip, std::uint16_t port);

        virtual ~Bot();

        void run(const std::atomic_bool &stopRequested);

    protected:

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

        std::size_t m_agentId{0};

        [[nodiscard]]
        virtual std::string_view getName() const = 0;

        virtual void think(
            const network::Protocol::Response& response
        ) = 0;

        [[nodiscard]]
        std::size_t getAgentId() const {
            return m_agentId;
        }

        virtual void move(core::Direction direction) final;
        virtual void inventory() final;
        virtual void look(core::Direction direction) final;
        virtual void take(int type) final;
        virtual void ritual() final;
        // virtual void attack(int agentId) final;

        [[nodiscard]]
        virtual std::optional<Decision> getPreviously() final;

    private:

        void addHistory(Decision decision);

        void sendDecision(
            network::Protocol::MessageType command,
            nlohmann::json payload
        );

        void sendCommand(
            network::Protocol::MessageType messageType,
            const std::string &payload
        ) const;

        void connect() const;
        void disconnect();
        void enqueue(network::Protocol::Response response);
        void receiveExactly(char *buffer, int size) const;
        [[nodiscard]] network::Protocol::Response receiveResponse() const;

        SOCKET m_socket{INVALID_SOCKET};

        std::string m_ip;
        std::uint16_t m_port;
        std::queue<network::Protocol::Response> m_queue;
        std::mutex m_mutex{};
        bool m_thinkStopped{false};
        std::condition_variable m_responseAvailable;
        std::deque<Decision> m_history{};
    };
}
