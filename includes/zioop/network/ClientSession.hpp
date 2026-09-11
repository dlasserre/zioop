#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "zioop/core/Agent.hpp"

namespace network {
    class Connection;

    class ClientSession {
    public:
        explicit ClientSession(Connection& connection);

        void addAgent(std::unique_ptr<core::Agent> agent);

        core::Agent* getAgent(std::size_t agentId);

    private:
        std::size_t m_clientId;
        Connection* m_connection;

        std::unordered_map<
            std::size_t,
            std::unique_ptr<core::Agent>
        > m_agents;
    };
}
