#include "zioop/network/ClientSession.hpp"

#include <optional>
#include <utility>

namespace network {

    namespace {
        std::size_t nextClientId = 0;
    }

    ClientSession::ClientSession(Connection &connection)
        : m_connection(&connection), m_clientId(nextClientId++) {
    }

    core::Agent* ClientSession::getAgent(std::size_t agentId) {
        const auto it = m_agents.find(agentId);
        if (it == m_agents.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    void ClientSession::addAgent(std::unique_ptr<core::Agent> agent) {
        const auto agentId = agent->getId();
        m_agents.emplace(agentId, std::move(agent));
    }
}
