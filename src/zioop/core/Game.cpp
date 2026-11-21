#include <utility>
#include <mutex>
#include <optional>
#include <type_traits>
#include <algorithm>
#include <cstdlib>
#include <variant>
#include <expected>

#include "zioop/core/Game.hpp"
#include "zioop/core/Error.hpp"
#include "zioop/core/Matrix.hpp"

namespace {

    template<typename T>
    struct is_expected: std::false_type {};

    template<typename T, typename E>
    struct is_expected<std::expected<T, E>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_expected_v = is_expected<std::remove_cvref_t<T>>::value;

    std::optional<core::Direction> directionFrom(const std::int64_t dx, const std::int64_t dy) {
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

    Agent *Game::findAgentUnlocked(const AgentId agentId) {
        return m_agentIndex[agentId].agent;
    }

    Agent *Game::findAgentUnlocked(const ClientId clientId, const AgentId agentId) {
        const auto &[clientEntryId, agent] = m_agentIndex[agentId];
        return clientEntryId == clientId ? agent : nullptr;
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
        } else if constexpr(is_expected_v<R>) {
            if (!agent) {
                return R{std::unexpected(Error{ErrorCode::AGENT_NOT_FOUND})};
            }
            return f(*agent);
        } else {
            if (!agent) return std::optional<R>{};
            return std::optional<R>{f(*agent)};
        }
    }

    void Game::tick(const std::chrono::steady_clock::time_point now) {
        std::scoped_lock lock_guard(m_stateMutex);

        m_matrix.repop(now);

        // @todo need to be refactor for future async events.
        for (const auto &[clientId, agents]: m_agents) {
            for (const auto &agent: agents) {
                if (agent->completeRitual(now)) {
                    agent->setFullLife();
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

    std::expected<GiveResult, Error> Game::give(
        const ClientId clientId,
        const AgentId agentId,
        const ResourceType type,
        const AgentId targetId
    ) {
        return withAgent(clientId, agentId, [this, &targetId, &type](Agent &agent) -> std::expected<GiveResult, Error> {
            const auto target = findAgentUnlocked(targetId);
            if (target == nullptr) {
                return std::unexpected(Error{ErrorCode::TARGET_NOT_FOUND});
            }

            if (target == &agent) {
                return std::unexpected(Error{ErrorCode::CANT_GIVE_YOURSELF});
            }

            if (agent.getCell() != target->getCell()) {
                return std::unexpected(Error{ErrorCode::TARGET_TOO_FAR});
            }

            const auto &result = agent.getInventory().give(type, target->getInventory());

            return GiveResult{
                .base = { .agentId = agent.getId()},
                .targetId = targetId,
                .status = result,
            };
        });
    }

    std::expected<TakenResult, Error> Game::take(
        const ClientId clientId,
        const AgentId agentId,
        const ResourceType type
    ) {
        return withAgent(clientId, agentId, [&type](Agent &agent) -> std::expected<TakenResult, Error> {
            const auto taken = agent.take(type);

            if (!taken) {
                return std::unexpected(Error{ErrorCode::CANT_TAKE_RESOURCE, static_cast<int>(type)});
            }

            return TakenResult{
                .base = { .agentId = agent.getId() },
                .inventory = agent.getItems()
            };
        });
    }

    std::optional<ClientId> Game::findOwnerUnlocked(const AgentId agentId) {
        for (const auto &[clientId, agents]: m_agents) {
            for (const auto &agent: agents) {
                if (agent->getId() == agentId) return clientId;
            }
        }
        return std::nullopt;
    }

    std::expected<AttackResult, Error> Game::attack(
        const ClientId clientId,
        const AgentId agentId,
        const AgentId targetId
    ) {
        return withAgent(clientId, agentId, [this, &targetId](Agent &agent) -> std::expected<AttackResult, Error> {
            const auto target = findAgentUnlocked(targetId);

            if (target == nullptr) {
                return std::unexpected(Error(ErrorCode::TARGET_TOO_FAR));
            }

            const auto result = agent.attack(target);

            switch (result) {
                case AttackStatus::RITUAL_IN_PROGRESS:
                case AttackStatus::SAME_OWNER:
                case AttackStatus::TARGET_NOT_FOUND:
                case AttackStatus::TARGET_TOO_STRONG:
                case AttackStatus::TOO_FAR:
                    return std::unexpected(Error(ErrorCode::TARGET_TOO_FAR, attackStatusErrorMessage(result)));

                case AttackStatus::KILLED:
                    m_matrix.removeAgent(*target);
                    emitUnlocked(Died{
                        .base = {.agentId = targetId, .clientId = target->getClientId()}
                    });
                    m_agentIndex.erase(targetId);
                    std::erase_if(m_agents.at(target->getClientId()), [targetId](const auto &a) {
                        return a->getId() == targetId;
                    });
                break;
                default: ;
            }
            return AttackResult{
                .base = { .agentId = agent.getId() },
                .targetId = targetId,
                .status = result
            };
        });
    }

    std::optional<Game::InitPlacement> Game::placeAgent(
        const ClientId clientId
    ) {
        auto agent = std::make_unique<Agent>(clientId);
        const auto position = m_matrix.addAgent(*agent);

        if (!position) {
            return std::nullopt;
        }

        const auto &agentId = agent->getId();
        m_agentIndex.emplace(agent->getId(), AgentEntry{
            .clientId = clientId,
            .agent = agent.get()
        });
        m_agents[clientId].push_back(std::move(agent));

        return InitPlacement{
            .agentId = agentId,
            .position = *position,
        };
    }

    std::expected<HealResult, Error> Game::heal(
        const ClientId clientId,
        AgentId agentId
    ) {
        return withAgent(clientId, agentId, [&agentId](Agent &agent) -> std::expected<HealResult, Error> {
            HealResult result;
            result.base = {.agentId = agentId};
            auto &inventory = agent.getInventory();

            if (!inventory.has(ResourceType::MAGIC_WATER)) {
                return std::unexpected(Error{ErrorCode::NO_WATER_ITEM});
            }

            inventory.remove(ResourceType::MAGIC_WATER, 1);
            agent.setFullLife();
            result.success = true;

            return result;
        });
    }

    std::expected<InvocationResult, Error> Game::instanciatAgent(
        const ClientId clientId,
        const AgentId agentId
    ) {
        return withAgent(clientId, agentId, [this, &clientId] (Agent &agent) -> std::expected<InvocationResult, Error> {
            auto &inventory = agent.getInventory();
            const auto &index = static_cast<std::size_t>(agent.getLevel() - 1);
            auto &cost = requirements[index];

            if (!inventory.hasAll(cost)) {
                return std::unexpected(Error{ErrorCode::NO_REQUIRED_RESOURCES});
            }

            const auto newAgent = placeAgent(clientId);
            if (!newAgent) {
                return std::unexpected(Error{ErrorCode::FAILED_TO_CREATE_MONSTER});
            }

            inventory.remove(cost);

            return InvocationResult{
                .base = {.agentId = agent.getId()},
                .spawnedAgentId = newAgent->agentId,
                .position = newAgent->position
            };
        });
    }

    std::expected<Game::InitPlacement, Error> Game::initClient(const ClientId clientId) {
        std::scoped_lock lock(m_stateMutex);

        if (m_agents.contains(clientId)) {
            return std::unexpected(Error{ErrorCode::CLIENT_ID_ALREADY_REGISTERED});
        }

        const auto &placement = placeAgent(clientId);
        if (!placement) {
            m_agents.erase(clientId);
        }
        return *placement;
    }

    std::expected<BroadcastResult, Error> Game::broadcast(
        const ClientId clientId,
        const AgentId agentId,
        const std::string &message
    ) {
        return withAgent(clientId, agentId, [&](Agent &speaker) -> std::expected<BroadcastResult, Error> {
            const auto cell = speaker.getCell();
            if (!cell) {
                return std::unexpected(Error{ErrorCode::CELL_NOT_FOUND});
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
        });
    }

    std::expected<MoveResult, Error> Game::move(
        const ClientId clientId,
        const AgentId agentId,
        const Direction direction
    ) {
        return withAgent(clientId, agentId,[&](Agent &agent) -> std::expected<MoveResult, Error> {

            const auto &position = agent.move(m_matrix, direction);
            if (!position) {
                return std::unexpected(Error{ErrorCode::FAILED_TO_MOVE});
            }
            return MoveResult{
                .base = {.agentId = agentId},
                .position = position
            };
        });
    }

    InventoryResult Game::inventory(
        const ClientId clientId,
        const AgentId agentId
    ) {
        return withAgent(clientId, agentId, [&](const Agent &agent) {
            const auto &items = agent.getItems();

            return InventoryResult{
                .base = { .agentId = agentId },
                .inventory = items
            };
        }).value();
    }

    std::expected<RitualResult, Error> Game::ritual(
        const ClientId clientId,
        const AgentId agentId
    ) {
        return withAgent(clientId, agentId, [&](Agent &agent) -> std::expected<RitualResult, Error> {
            RitualResult result{
                .base = {.agentId = agentId}
            };

            if (agent.isRitualInProgress()) {
                return std::unexpected(Error{ErrorCode::RITUAL_ALREADY_IN_PROGRESS});
            }

            const auto duration = RitualTimePerLevel * (agent.getLevel() + 1);
            if (!agent.startRitual(duration, std::chrono::steady_clock::now())) {
                return std::unexpected(Error{ErrorCode::NOT_ENOUGH_RESOURCES});
            }

            return result;
        });
    }

    std::expected<LookResult, Error> Game::look(
        const ClientId clientId,
        const AgentId agentId,
        const Direction direction
    ) {
        return withAgent(clientId, agentId, [&](Agent &agent) -> std::expected<LookResult, Error> {
            const auto result = agent.look(m_matrix, direction);

            if (!result) {
                return std::unexpected(Error{ErrorCode::NOT_YET_READY});
            }
            if (result) {
                agent.recordLook(direction);
            }
            return *result;
        });
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

    std::optional<std::chrono::steady_clock::time_point> Game::reserve(
        const ClientId clientId,
        const AgentId agentId,
        const GameCommand command
    ) {
        return withAgent(clientId, agentId, [&](Agent &agent) {
            return agent.reserve(
                commandDuration(command),
                std::chrono::steady_clock::now());
        });
    }

    std::optional<ClientId> Game::getAgentClientId(const AgentId agentId) {
        std::scoped_lock lock_guard(m_stateMutex);
        const auto agent = findAgentUnlocked(agentId);
        if (!agent) {
            return std::nullopt;
        }

        return agent->getClientId();
    }

    void Game::removeClientAgent(const ClientId clientId) {
        std::scoped_lock lock_guard(m_stateMutex);

        const auto it = m_agents.find(clientId);
        if (it == m_agents.end()) {
            return;
        }

        for (const auto &agent: it->second) {
            m_agentIndex.erase(agent->getId());
            m_matrix.removeAgent(*agent);
        }
        m_agents.erase(it);
    }
}
