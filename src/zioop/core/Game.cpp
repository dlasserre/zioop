#include <utility>
#include <functional>
#include <mutex>
#include <optional>
#include <type_traits>
#include <algorithm>
#include <cstdlib>
#include <ranges>

#include "zioop/core/Game.hpp"
#include "zioop/core/Matrix.hpp"

namespace {

    std::optional<core::Direction> directionFrom(const long long dx, const long long dy) {
        if (dx == 0 && dy == 0) {
            return std::nullopt;
        }
        if (std::abs(dx) > std::abs(dy)) {
            return dx > 0 ? core::Direction::EAST : core::Direction::WEST;
        }
        return dy > 0 ? core::Direction::SOUTH : core::Direction::NORTH;
    }
}

namespace core {
    constexpr int DAMAGE = 2;
    constexpr int BroadCastRange = 3;
    constexpr auto SpeechDuration = std::chrono::seconds(4);

    Game::Game(Matrix matrix)
        : m_matrix(std::move(matrix)) {
    }

    std::size_t Game::countMonsters(const ClientId clientId) {
        const auto it = m_agents.find(clientId);
        return it == m_agents.end() ? 0 : it->second.size();
    }

    Agent *Game::findAgentUnlocked(AgentId agentId) {
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

    Agent *Game::findAgentUnlocked(const ClientId clientId, AgentId agentId) {
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
    auto Game::withAgent(const ClientId clientId, const AgentId agentId, F &&f) {
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

        m_matrix.repop(now); // Repop des resources.

        // @todo need to be refactor for future async events.
        for (const auto &[clientId, agents]: m_agents) {
            for (const auto &agent: agents) {
                if (agent->completeRitual(now)) {
                    agent->setHealth(FULL_LIFE);
                    emitUnlocked(RitualCompleted{
                        .base = {.agentId = agent->getId(), .clientId = clientId},
                        .level = agent->getLevel()
                    });
                }

                const auto rottedResources = agent->getInventory().update(now);
                if (!rottedResources.empty()) {
                    emitUnlocked(DeletedRottenResources{
                        .base = {.agentId = agent->getId(), .clientId = clientId},
                        .resources = rottedResources
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

    std::optional<GiveResult> Game::give(
        const ClientId clientId,
        const AgentId agentId,
        const ResourceType type,
        const AgentId targetId
    ) {
        return withAgent(clientId, agentId, [&](Agent &agent) -> std::optional<GiveResult> {
            const auto target = findAgentUnlocked(targetId);
            if (target == nullptr) {
                return GiveResult::TARGET_NOT_FOUND;
            }

            if (target == &agent) {
                return GiveResult::CANT_GIVE_YOURSELF;
            }

            if (agent.getCell() != target->getCell()) {
                return GiveResult::TARGET_TO_FAR;
            }

            return agent.getInventory().give(type, target->getInventory())
                       ? GiveResult::GIVED
                       : GiveResult::RESOURCE_NOT_FOUND;
        }).value_or(std::nullopt);
    }

    std::optional<Game::TakeResult> Game::take(
        const ClientId clientId,
        const AgentId agentId,
        const ResourceType type
    ) {
        return withAgent(clientId, agentId, [&](Agent &agent) -> std::optional<TakeResult> {
            return TakeResult{
                .taken = agent.take(type),
                .inventory = agent.getItems()
            };
        }).value_or(std::nullopt);
    }

    std::optional<ClientId> Game::findOwnerUnlocked(const AgentId agentId) {
        for (const auto &[clientId, agents]: m_agents) {
            for (const auto &agent: agents) {
                if (agent->getId() == agentId) return clientId;
            }
        }
        return std::nullopt;
    }

    std::optional<AttackResult> Game::attack(
        const ClientId clientId,
        const AgentId agentId,
        const AgentId targetId
    ) {
        return withAgent(clientId, agentId, [&](Agent &agent) -> std::optional<AttackResult> {
            const auto target = findAgentUnlocked(targetId);
            if (target == nullptr) {
                return AttackResult::TARGET_NOT_FOUND;
            }
            const auto result = agent.attack(target);
            if (AttackResult::KILLED == result) {
                m_matrix.removeAgent(*target);
                emitUnlocked(Died{
                    .base = {.agentId = targetId, .clientId = target->getClientId()}
                });
                std::erase_if(m_agents.at(target->getClientId()), [&](const auto &a) {
                    return a->getId() == targetId;
                });
            }
            return result;
        }).value_or(std::nullopt);
    }

    std::optional<Game::InitPlacement> Game::placeAgent(
        const ClientId clientId
    ) {
        auto [it, _] = m_agents.try_emplace(clientId);
        auto agent = std::make_unique<Agent>(clientId);

        const auto position = m_matrix.addAgent(*agent);

        if (!position) {
            return std::nullopt;
        }
        it->second.push_back(std::move(agent));

        return InitPlacement{
            .agentId = it->second.back()->getId(),
            .position = *position
        };
    }

    std::optional<InvocationResult> Game::instanciatAgent(
        const ClientId clientId,
        const AgentId agentId
    ) {
        return withAgent(clientId, agentId, [&](Agent &agent) -> std::optional<InvocationResult> {
            auto &inventory = agent.getInventory();
            const auto &index = static_cast<std::size_t>(agent.getLevel() - 1);

            auto &cost = requirements[index];
            if (const auto removed = inventory.remove(cost); !removed) {
                return std::nullopt;
            }

            const auto newAgent = placeAgent(clientId);
            if (!newAgent) {
                return std::nullopt;
            }

            return InvocationResult{
                .base = {.agentId = agent.getId()},
                .spawnedAgentId = newAgent->agentId,
                .position = newAgent->position
            };
        }).value_or(std::nullopt);
    }

    std::optional<Game::InitPlacement> Game::initClient(const ClientId clientId) {
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
        const ClientId clientId,
        const AgentId agentId,
        std::string &message
    ) {
        return withAgent(clientId, agentId, [&](Agent &speaker) -> std::optional<BroadcastResult> {
            const auto cell = speaker.getCell();
            if (!cell) {
                return std::nullopt;
            }
            const auto [x, y] = cell->getPosition();
            speaker.say(message, std::chrono::steady_clock::now() + SpeechDuration);

            for (const auto &[listenerClientId, agents]: m_agents) {
                for (const auto &listener: agents) {
                    if (listener->getId() == agentId || !listener->getCell()) {
                        continue;
                    }
                    const auto position = listener->getCell()->getPosition();
                    const auto dx = static_cast<long long>(x) - static_cast<long long>(position.x);
                    const auto dy = static_cast<long long>(y) - static_cast<long long>(position.y);

                    if (std::max(std::abs(dx), std::abs(dy)) > BroadCastRange) continue;

                    emitUnlocked(Message{
                        .base = {.agentId = listener->getId(), .clientId = listenerClientId},
                        .speakerId = agentId,
                        .direction = directionFrom(dx, dy),
                        .message = message
                    });
                }
            }

            return BroadcastResult{
                .base = {.agentId = agentId}
            };
        }).value_or(std::nullopt);
    }

    std::optional<MoveResult> Game::move(
        const ClientId clientId,
        const AgentId agentId,
        const Direction direction
    ) {
        return withAgent(clientId, agentId,[&](Agent &agent) -> std::optional<MoveResult> {
            return MoveResult{
                .base = {.agentId = agentId},
                .position = agent.move(m_matrix, direction)
            };
        }).value_or(std::nullopt);
    }

    std::optional<InventoryResult> Game::inventory(
        const ClientId clientId,
        const AgentId agentId
    ) {
        return withAgent(clientId, agentId, [&](const Agent &agent) -> std::optional<InventoryResult> {
            return InventoryResult{
                .base = {.agentId = agentId},
                .inventory = agent.getItems()
            };
        }).value_or(std::nullopt);
    }

    std::optional<RitualResult> Game::ritual(
        const ClientId clientId,
        const AgentId agentId
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
        const ClientId clientId,
        const AgentId agentId,
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

        std::unordered_map<AgentId, ClientId> owners;
        for (const auto &[clientId, agents]: m_agents) {
            for (const auto &agent: agents) {
                owners.emplace(agent->getId(), clientId);
            }
        }

        for (const auto &column: m_matrix.getMatrix()) {
            for (const auto &cell: column) {
                for (const auto &[subCell, type]: cell->getResources()) {
                    result.resources.push_back({
                        .position = cell->getPosition(),
                        .subCell = subCell,
                        .type = type
                    });
                }

                for (const auto &agent: cell->getAgents()) {
                    if (!agent) continue;
                    AgentSnapshot snapshot{
                        .agentId = agent->getId(),
                        .teamId = agent->getClientId(),
                        .health = agent->getHealth(),
                        .level = agent->getLevel(),
                        .position = cell->getPosition(),
                        .subCell = cell->getSubCell(*agent).value_or(0),
                        .message = agent->getSpeech(now),
                        .ritual = agent->isRitualInProgress(),
                        .lookDirection = agent->getLookDirection(),
                        .lookCount = agent->getLookCount(),
                        .attackTarget = agent->getAttackTarget(),
                        .attackCount = agent->getAttackCount()
                    };
                    for (const auto &[type, quantity]: agent->getItems()) {
                        snapshot.inventory.push_back({.type = type, .quantity = quantity});
                    }

                    result.agents.push_back(snapshot);
                }
            }
        }

        return result;
    }

    void Game::removeClientAgent(const ClientId clientId) {
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
