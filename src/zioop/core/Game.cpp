#include <utility>

#include "zioop/core/Game.hpp"
#include "zioop/core/Matrix.hpp"

namespace core {
    Game::Game(Matrix matrix)
        : m_matrix(std::move(matrix)) {
    }

    std::vector<Game::RitualCompleted> Game::tick(
        const std::chrono::steady_clock::time_point now
    ) {
        std::lock_guard<std::mutex> lock_guard(m_stateMutex);
        m_matrix.updateResources();

        std::vector<RitualCompleted> completed;
        for (const auto &[agentId, entry]: m_agents) {
            if (entry.agent->completeRitual(now)) {
                completed.push_back({
                    .clientId = entry.clientId,
                    .agentId = agentId,
                    .level = entry.agent->getLevel()
                });
            }
        }
        return completed;
    }

    std::optional<Game::TakeResult> Game::take(
        const std::size_t clientId,
        const std::size_t agentId,
        const ResourceType type
    ) {
        std::lock_guard<std::mutex> lock_guard(m_stateMutex);
        const auto it = m_agents.find(agentId);
        if (it == m_agents.end() || it->second.clientId != clientId) {
            return std::nullopt;
        }

        Agent &agent = *it->second.agent;

        return TakeResult{
            .taken = agent.take(type),
            .inventory = agent.getInventory()
        };
    }

    std::optional<Game::AttackResult> Game::attack(
        const std::size_t clientId,
        const std::size_t agentId,
        const std::size_t targetId
    ) {
        std::lock_guard<std::mutex> lock_guard(m_stateMutex);

        const auto it = m_agents.find(agentId);
        if (it == m_agents.end() || it->second.clientId != clientId) {
            return std::nullopt;
        }
        const Agent &agent = *it->second.agent;
        if (agent.isRitualInProgress()) {
            return AttackResult::RITUAL_IN_PROGRESS;
        }

        const auto targetIt = m_agents.find(targetId);
        if (targetIt == m_agents.end()) {
            return AttackResult::TARGET_NOT_FOUND;
        }

        if (targetIt->second.clientId == clientId) {
            return AttackResult::SAME_OWNER;
        }
        Agent &target = *targetIt->second.agent;
        if (!agent.getCell() ||agent.getCell() != target.getCell()) {
            return AttackResult::TOO_FAR;
        }

        if (agent.getLevel() < target.getLevel()) {
            return AttackResult::TARGET_TOO_STRONG;
        }

        m_matrix.removeAgent(target);
        m_agents.erase(targetId);

        return AttackResult::KILLED;
    }

    std::optional<Game::AgentPlacement> Game::addAgent(
        const std::size_t clientId,
        const ANGLE angle
    ) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto agent = std::make_unique<Agent>();
        const auto agentId = agent->getId();

        auto [it, inserted] = m_agents.try_emplace(
            agentId,
            AgentEntry{
                .clientId = clientId,
                .agent = std::move(agent)
            }
        );

        if (!inserted) {
            return std::nullopt;
        }

        const auto position = m_matrix.addAgent(
            *it->second.agent,
            angle
        );

        if (!position) {
            m_agents.erase(it);
            return std::nullopt;
        }

        return AgentPlacement{
            .agentId = agentId,
            .position = *position
        };
    }

    std::optional<Position> Game::move(
        std::size_t clientId,
        std::size_t agentId,
        Direction direction
    ) {
        std::lock_guard<std::mutex> lock_guard(m_stateMutex);

        const auto it = m_agents.find(agentId);

        if (it == m_agents.end() || it->second.clientId != clientId) {
            return std::nullopt;
        }

        return it->second.agent->move(m_matrix, direction);
    }

    std::optional<std::vector<InventoryResource> > Game::inventory(
        const std::size_t clientId,
        const std::size_t agentId
    ) {
        std::lock_guard<std::mutex> lock_guard(m_stateMutex);
        const auto it = m_agents.find(agentId);

        if (it == m_agents.end() || it->second.clientId != clientId) {
            return std::nullopt;
        }

        return it->second.agent->getInventory();
    }

    std::optional<Game::RitualStartResult> Game::ritual(
        const std::size_t clientId,
        const std::size_t agentId
    ) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        const auto it = m_agents.find(agentId);
        if (it == m_agents.end() || it->second.clientId != clientId) {
            return std::nullopt;
        }

        Agent &agent = *it->second.agent;
        if (agent.isRitualInProgress()) {
            return RitualStartResult::ALREADY_IN_PROGRESS;
        }

        const auto duration = RitualTimePerLevel * (agent.getLevel() + 1);
        if (!agent.startRitual(RitualFruitCost, duration, std::chrono::steady_clock::now())) {
            return RitualStartResult::NOT_ENOUGH_RESOURCES;
        }
        return RitualStartResult::STARTED;
    }

    std::optional<LookResult> Game::look(
        const std::size_t clientId,
        const std::size_t agentId,
        const Direction direction
    ) const {
        std::lock_guard<std::mutex> lock_guard(m_stateMutex);

        const auto it = m_agents.find(agentId);

        if (it == m_agents.end() || it->second.clientId != clientId) {
            return std::nullopt;
        }

        return it->second.agent->look(m_matrix, direction);
    }

    MatrixSnapshot Game::getMatrix() const {
        std::lock_guard<std::mutex> lock_guard(m_stateMutex);

        MatrixSnapshot result{
            .sizeX = m_matrix.getSizeX(),
            .sizeY = m_matrix.getSizeY()
        };

        for (const auto &column: m_matrix.getMatrix()) {
            for (const auto &cell: column) {
                for (const auto &resource: cell->getResources()) {
                    result.resources.push_back({
                        .position = cell->getPosition(),
                        .subCell = resource.subCell,
                        .type = resource.type,
                        .rotting = resource.rotting
                    });
                }
            }
        }

        for (const auto &[agentId, entry]: m_agents) {
            const auto *cell = entry.agent->getCell();
            if (!cell) {
                continue;
            }
            result.agents.push_back({
                .agentId = agentId,
                .position = cell->getPosition(),
                .subCell = cell->getSubCell(*entry.agent).value_or(0)
            });
        }

        return result;
    }

    void Game::removeClientAgent(std::size_t clientId) {
        std::lock_guard<std::mutex> lock_guard(m_stateMutex);

        for (auto it = m_agents.begin(); it != m_agents.end();) {
            if (it->second.clientId == clientId) {
                m_matrix.removeAgent(*it->second.agent);
                it = m_agents.erase(it);
            } else {
                ++it;
            }
        }
    }
}
