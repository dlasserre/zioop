#include <utility>
#include <functional>
#include <mutex>
#include <optional>
#include <type_traits>
#include <algorithm>
#include <cstdlib>

#include "zioop/core/Game.hpp"

#include <ranges>

#include "zioop/core/Matrix.hpp"


namespace {
    std::optional<core::Direction> directionFrom(const long long dx, const long long dy) {
        if (dx == 0 && dy == 0) {
            return std::nullopt;
        }
        if (std::abs(dx) >  std::abs(dy)) {
            return dx > 0 ? core::Direction::EAST : core::Direction::WEST;
        }
        return dy > 0 ? core::Direction::SOUTH: core::Direction::NORTH;
    }
}

namespace core {
    constexpr int DAMAGE = 2;
    constexpr int BroadCastRange = 3;
    constexpr auto SpeechDuration = std::chrono::seconds(4);
    constexpr int MAX_MONSTERS = 5;

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

    void Game::tick(const std::chrono::steady_clock::time_point now) {
        std::scoped_lock lock_guard(m_stateMutex);
        m_matrix.updateResources();

        // @todo need to be refactor for future async events.
        for (const auto &[clientId, agents]: m_agents) {
            for (const auto &agent: agents) {
                if (agent->completeRitual(now)) {
                    agent->setHealth(FULL_LIFE);
                    emitUnlocked(RitualCompleted{
                        .base = {.agentId =  agent->getId(), .clientId = clientId},
                        .level = agent->getLevel()
                    });
                }
            }
        }
    }

    void Game::emitUnlocked(Event event) {
        m_events.push(std::move(event));
    }

    std::vector<Event> Game::drainEvents() {
        std::lock_guard lock(m_stateMutex);
        std::vector<Event> events;
        events.reserve(m_events.size());
        while (!m_events.empty()) {
            events.push_back(std::move(m_events.front()));
            m_events.pop();
        }

        return events;
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

    std::optional<std::size_t> Game::findOwnerUnlocked(std::size_t agentId) {
        for (const auto &[clientId, agents] : m_agents) {
            for (const auto &agent : agents) {
                if (agent->getId() == agentId) return clientId;
            }
        }
        return std::nullopt;
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

            const auto owner = findOwnerUnlocked(targetId);
            if (!*owner) {
                return AttackResult::TARGET_NOT_FOUND;
            }
            if (*owner == clientId) {
                return AttackResult::SAME_OWNER;
            }

            target->damage(agent.getLevel() * DAMAGE);
            if (target->getHealth() == 0) {
                m_matrix.removeAgent(*target);
                emitUnlocked(Died{
                    .base = { .agentId = targetId, .clientId = *owner}
                });
                std::erase_if(m_agents.at(*owner), [&](const auto &a) {
                    return a->getId() == targetId;
                });
            }
            return target->isAlive() ? AttackResult::DAMAGED : AttackResult::KILLED;
        }).value_or(std::nullopt);
    }

    std::optional<Game::InitPlacement> Game::placeAgent(
        const std::size_t clientId
    ) {
        auto [it, _] = m_agents.try_emplace(clientId);

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

    std::optional<InvocationResult> Game::createAgent(
        const std::size_t clientId,
        const std::size_t agentId
    ){
        // Add check resource to instanciat a new agent.
        // Need table resources
        return withAgent(clientId, agentId, [&](const Agent &agent) -> std::optional<InvocationResult> {
            const auto newAgent = placeAgent(clientId);
            if (!newAgent) {
                return std::nullopt; // Matrix is full
            }
            return InvocationResult{
                .base = { .agentId = agent.getId() },
                .spawnedAgentId = newAgent->agentId,
                .position = newAgent->position
            };
        }).value_or(std::nullopt);
    }

    std::optional<Game::InitPlacement> Game::initClient(const std::size_t clientId) {
        std::scoped_lock lock(m_stateMutex);

        if (m_agents.contains(clientId)) {
            return std::nullopt;
        }

        const auto placement = placeAgent(clientId);
        if (!placement) {
            m_agents.erase(clientId);
        }
        return placement;
    }

    std::optional<BroadcastResult> Game::broadcast(
        const std::size_t clientId,
        const std::size_t agentId,
        const std::string &message
    ) {

        return withAgent(clientId, agentId, [&](Agent &speaker) -> std::optional<BroadcastResult> {
            const auto cell = speaker.getCell();
            if (!cell) {
                return std::nullopt;
            }
            const auto origin = cell->getPosition();
            speaker.say(message, std::chrono::steady_clock::now() + SpeechDuration);

            for (const auto &[listenerClientId, agents]: m_agents) {
                for (const auto &listener: agents) {
                    if (listener->getId() == agentId || !listener->getCell()) {
                        continue;
                    }
                    const auto position = listener->getCell()->getPosition();
                    const auto dx = static_cast<long long>(origin.x) - static_cast<long long>(position.x);
                    const auto dy = static_cast<long long>(origin.y) - static_cast<long long>(position.y);

                    if (std::max(std::abs(dx), std::abs(dy)) > BroadCastRange) continue;

                    emitUnlocked(Message{
                        .base = { .agentId = listener->getId(), .clientId = listenerClientId},
                        .speakerId = agentId,
                        .direction = directionFrom(dx, dy),
                        .message = message
                    });
                }
            }

            return BroadcastResult{
                .base = { .agentId = agentId }
            };
        }).value_or(std::nullopt);
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
        return withAgent(clientId, agentId, [&](Agent &agent) -> std::optional<LookResult> {
            auto result = agent.look(m_matrix, direction);
            if (result) {
                agent.recordLook(direction);
            }
            return result;
        }).value_or(std::nullopt);
    }

    MatrixSnapshot Game::getMatrix() const {
        std::scoped_lock lock_guard(m_stateMutex);

        const auto now = std::chrono::steady_clock::now();
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
                        .message = agent->getSpeech(now),
                        .ritual = agent->isRitualInProgress(),
                        .lookDirection = agent->getLookDirection(),
                        .lookCount = agent->getLookCount()
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
