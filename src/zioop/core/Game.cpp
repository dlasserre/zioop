#include <utility>
#include <functional>
#include <mutex>
#include <optional>
#include <type_traits>

#include "zioop/core/Game.hpp"

#include <ranges>

#include "zioop/core/Matrix.hpp"

namespace core {
    constexpr int DAMAGE = 2;

    Game::Game(Matrix matrix)
        : m_matrix(std::move(matrix)) {
    }

    Agent *Game::findAgentUnlocked(std::size_t agentId) {
        for (const auto &val: m_agents | std::views::values) {
            auto found = std::ranges::find_if(
                val.begin(),
                val.end(),
                [&](const auto &agent) {
                    return agent->getId() == agentId;
                }
            );
            if (found != val.end()) {
                return found->get();
            }
        }

        return nullptr;
    }

    Agent *Game::findAgentUnlocked(const std::size_t clientId, std::size_t agentId) {
        const auto it = m_agents.find(clientId);

        if (it == m_agents.end() || it->first != clientId) {
            return nullptr;
        }

        const auto ait = std::ranges::find_if(
            it->second.begin(),
            it->second.end(),
            [&](const auto &agent) {
                return agent->getId() == agentId;
            });

        if (ait == it->second.end()) {
            return nullptr;
        }

        return ait->get();
    }

    template<typename F>
    auto Game::withAgent(const std::size_t clientId, const std::size_t agentId, F &&f) {
        using R = std::invoke_result_t<F, Agent &>;

        std::scoped_lock lock(m_stateMutex);
        Agent *agent = findAgentUnlocked(clientId, agentId);

        if constexpr (std::is_void_v<R>) {
            if (!agent) return false;
            f(*agent);
            return true;
        } else {
            if (!agent) return std::optional<R>{};
            return std::optional<R>{f(*agent)};
        }
    }

    std::vector<Game::RitualCompleted> Game::tick(
        const std::chrono::steady_clock::time_point now
    ) {
        std::scoped_lock lock_guard(m_stateMutex);
        m_matrix.updateResources();

        std::vector<RitualCompleted> completed;
        for (const auto &[clientId, agents]: m_agents) {
            for (const auto &agent: agents) {
                if (agent->completeRitual(now)) {
                    completed.push_back({
                        .clientId = clientId,
                        .agentId = agent->getId(),
                        .level = agent->getLevel()
                    });
                }
            }
        }
        return completed;
    }

    std::optional<Game::TakeResult> Game::take(
        const std::size_t clientId,
        const std::size_t agentId,
        const ResourceType type
    ) {
        return withAgent(clientId, agentId, [&](Agent &agent) -> std::optional<TakeResult> {
            return TakeResult{
                .taken = agent.take(type),
                .inventory = agent.getInventory()
            };
        }).value_or(std::nullopt);
    }

    std::optional<AttackResult> Game::attack(
        const std::size_t clientId,
        const std::size_t agentId,
        const std::size_t targetId
    ) {
        return withAgent(clientId, agentId, [&](const Agent &agent) -> std::optional<AttackResult> {
            if (agent.isRitualInProgress()) {
                return AttackResult::RITUAL_IN_PROGRESS;
            }

            const auto target = findAgentUnlocked(targetId);
            if (target == nullptr) {
                return AttackResult::TARGET_NOT_FOUND;
            }

            if (!agent.getCell() || agent.getCell() != target->getCell()) {
                return AttackResult::TOO_FAR;
            }

            if (agent.getLevel() < target->getLevel()) {
                return AttackResult::TARGET_TOO_STRONG;
            }

            target->damage(agent.getLevel() * DAMAGE);
            if (target->getHealth() == 0) {
                m_matrix.removeAgent(*target);
                m_agents.erase(targetId);
                return AttackResult::KILLED;
            }

            return AttackResult::DAMAGED;
        }).value_or(std::nullopt);
    }

    std::optional<Game::InitPlacement> Game::placeAgent(
        std::vector<std::unique_ptr<Agent> > &agents
    ) {
        auto agent = std::make_unique<Agent>();

        const auto position = m_matrix.addAgent(
            *agent
        );

        if (!position) {
            return std::nullopt;
        }
        agents.push_back(std::move(agent));

        return InitPlacement{
            .agentId = agent->getId(),
            .position = *position
        };
    }

    std::optional<Game::InitPlacement> Game::placeAgent(
        const std::size_t clientId
    ) {
        auto [it, _] = m_agents.try_emplace(clientId);

        it->second.push_back(std::make_unique<Agent>());

        const auto position = m_matrix.addAgent(
            *it->second.back()
        );

        return InitPlacement{
            .agentId = it->second.back()->getId(),
            .position = *position
        };
    }

    std::optional<Game::InitPlacement> Game::createAgent(
        const std::size_t clientId
    ) {
        std::scoped_lock lock(m_stateMutex);

        const auto it = m_agents.find(clientId);

        if (it == m_agents.end()) {
            return std::nullopt;
        }

        it->second.push_back(std::make_unique<Agent>());

        const auto position = m_matrix.addAgent(
            *it->second.back()
        );

        return InitPlacement{
            .agentId = it->second.back()->getId(),
            .position = *position
        };
    }

    std::optional<Game::InitPlacement> Game::initClient(
        const std::size_t clientId
    ) {
        std::scoped_lock lock(m_stateMutex);

        auto [it, inserted] = m_agents.try_emplace(clientId);

        if (!inserted) {
            return std::nullopt;
        }
        it->second.push_back(std::make_unique<Agent>());

        const auto position = m_matrix.addAgent(
            *it->second.back()
        );

        if (!position) {
            m_agents.erase(it);
            return std::nullopt;
        }

        return InitPlacement{
            .agentId = it->second.back()->getId(),
            .position = *position
        };
    }

    std::optional<MoveResult> Game::move(
        const std::size_t clientId,
        const std::size_t agentId,
        const Direction direction
    ) {
        return withAgent(clientId, agentId,
        [&](Agent &agent) -> std::optional<MoveResult> {
             return MoveResult{
                 .base = {.agentId = agentId},
                 .position = agent.move(m_matrix, direction)
             };
        }).value_or(std::nullopt);
    }

    std::optional<InventoryResult> Game::inventory(
        const std::size_t clientId,
        const std::size_t agentId
    ) {
        return withAgent(clientId, agentId, [&](const Agent &agent) -> std::optional<InventoryResult> {
            return InventoryResult{
                .base = { .agentId = agentId },
                .inventory = agent.getInventory()
            };
        }).value_or(std::nullopt);
    }

    std::optional<RitualResult> Game::ritual(
        const std::size_t clientId,
        const std::size_t agentId
    ) {
        return withAgent(clientId, agentId, [&](Agent &agent) -> std::optional<RitualResult> {
            RitualResult result{
                .base = {.agentId = agentId}
            };
            if (agent.isRitualInProgress()) {
                result.result = RitualStartResult::ALREADY_IN_PROGRESS;
                return result;
            }
            const auto duration = RitualTimePerLevel * (agent.getLevel() + 1);
            if (!agent.startRitual(duration, std::chrono::steady_clock::now())) {
                result.result = RitualStartResult::NOT_ENOUGH_RESOURCES;
            }
            return result;
        }).value_or(std::nullopt);
    }

    std::optional<LookResult> Game::look(
        const std::size_t clientId,
        const std::size_t agentId,
        const Direction direction
    ) {
        return withAgent(clientId, agentId, [&](const Agent &agent) -> std::optional<LookResult> {
            return agent.look(m_matrix, direction);
        }).value_or(std::nullopt);
    }

    MatrixSnapshot Game::getMatrix() const {
        std::scoped_lock lock_guard(m_stateMutex);

        MatrixSnapshot result{
            .sizeX = m_matrix.getSizeX(),
            .sizeY = m_matrix.getSizeY()
        };

        for (const auto &column: m_matrix.getMatrix()) {
            for (const auto &cell: column) {
                for (const auto &[subCell, type, rotting]: cell->getResources()) {
                    result.resources.push_back({
                        .position = cell->getPosition(),
                        .subCell = subCell,
                        .type = type,
                        .rotting = rotting
                    });
                }

                for (const auto &agent: cell->getAgents()) {
                    if (!agent) continue;
                    AgentSnapshot snapshot{
                        .agentId = agent->getId(),
                        .health = agent->getHealth(),
                        .level = agent->getLevel(),
                        .position = cell->getPosition(),
                        .subCell = cell->getSubCell(*agent).value_or(0),
                    };
                    for (const auto &[type, quantity]: agent->getInventory()) {
                        snapshot.inventory.push_back({.type = type, .quantity = quantity});
                    }

                    result.agents.push_back(snapshot);
                }
            }
        }

        return result;
    }

    void Game::removeClientAgent(const std::size_t clientId) {
        std::scoped_lock lock_guard(m_stateMutex);

        const auto it = m_agents.find(clientId);
        if (it == m_agents.end()) {
            return;
        }

        for (const auto &agent: it->second) {
            m_matrix.removeAgent(*agent);
        }
        m_agents.erase(it);
    }
}
