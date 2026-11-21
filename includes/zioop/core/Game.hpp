#pragma once

#include <chrono>
#include <mutex>
#include <queue>
#include <memory>
#include <optional>
#include <unordered_map>
#include <array>
#include <expected>

#include "Error.hpp"
#include "zioop/core/Event.hpp"
#include "zioop/core/Inventory.hpp"
#include "zioop/core/Matrix.hpp"
#include "zioop/core/Agent.hpp"
#include "zioop/core/Results.hpp"
#include "zioop/core/MatrixSnapshot.hpp"

namespace core {

    constexpr int MAX_MONSTERS = 5;

    inline constexpr std::array<ResourceCount, MAX_MONSTERS> requirements{
        {
            {20, 5, 1, 0},
            {30, 6, 2, 0},
            {40, 7, 3, 0},
            {50, 8, 4, 0},
            {60, 9, 5, 0}
        }
    };

    enum class GameCommand { MOVE, LOOK, TAKE, INVENTORY, RITUAL, ATTACK, INVOCATION, BROADCAST, GIVE, HEAL };

    constexpr std::chrono::milliseconds commandDuration(const GameCommand command) {
        using namespace std::chrono_literals;
        switch (command) {
            case GameCommand::MOVE: return 700ms;
            case GameCommand::HEAL:
            case GameCommand::LOOK:
            case GameCommand::TAKE:
            case GameCommand::GIVE: return 300ms;
            case GameCommand::ATTACK: return 500ms;
            case GameCommand::BROADCAST: return 200ms;
            case GameCommand::INVOCATION: return 1000ms;
            case GameCommand::INVENTORY:
            case GameCommand::RITUAL: return 0ms;
        }
        return 0ms;
    }

    class Game {
    public:
        static constexpr std::chrono::seconds RitualTimePerLevel{30};

        struct InitPlacement {
            AgentId agentId;
            Position position;
        };

        struct TakeResult {
            bool taken{false};
            std::vector<InventoryResource> inventory;
        };

        std::vector<Event> drainEvents();

        explicit Game(Matrix matrix);

        void tick(std::chrono::steady_clock::time_point now);

        [[nodiscard]]
        std::expected<InitPlacement, Error> initClient(
            ClientId clientId
        );

        std::expected<InvocationResult, Error> instanciatAgent(
            ClientId clientId,
            AgentId agentId
        );

        [[nodiscard]]
        std::expected<MoveResult, Error> move(
            ClientId clientId,
            AgentId agentId,
            Direction direction
        );

        [[nodiscard]]
        std::optional<std::chrono::steady_clock::time_point> reserve(
            ClientId clientId,
            AgentId agentId,
            GameCommand command
        );

        [[nodiscard]]
        std::expected<BroadcastResult, Error> broadcast(
            ClientId clientId,
            AgentId agentId,
            const std::string &message
        );

        [[nodiscard]]
        std::expected<LookResult, Error> look(
            ClientId clientId,
            AgentId agentId,
            Direction direction
        );

        [[nodiscard]]
        std::expected<TakenResult, Error> take(
            ClientId clientId,
            AgentId agentId,
            ResourceType type
        );

        [[nodiscard]]
        std::expected<GiveResult, Error> give(
            ClientId clientId,
            AgentId agentId,
            ResourceType type,
            AgentId targetId
        );

        [[nodiscard]]
        InventoryResult inventory(
            ClientId clientId,
            AgentId agentId
        );

        [[nodiscard]]
        std::expected<RitualResult, Error> ritual(
            ClientId clientId,
            AgentId agentId
        );

        [[nodiscard]]
        std::expected<AttackResult, Error> attack(
            ClientId clientId,
            AgentId agentId,
            AgentId targetId
        );

        [[nodiscard]]
        std::expected<HealResult, Error> heal(
            ClientId clientId,
            AgentId agentId
        );

        std::optional<ClientId> getAgentClientId(AgentId agentId);

        MatrixSnapshot getMatrix() const;

        void removeClientAgent(ClientId clientId);

        [[nodiscard]] std::size_t countMonsters(ClientId clientId);
    private:

        struct AgentEntry {
            ClientId clientId;
            Agent* agent;
        };

        void emitUnlocked(Event event);

        std::optional<ClientId> findOwnerUnlocked(AgentId agentId);

        std::queue<Event> m_events;

        Agent* findAgentUnlocked(ClientId clientId, AgentId agentId);
        Agent* findAgentUnlocked(AgentId agentId);

        template <typename F>
        auto withAgent(ClientId clientId, AgentId agentId, F&& f);

        std::optional<InitPlacement> placeAgent(ClientId clientId);

        mutable std::mutex m_stateMutex;
        Matrix m_matrix;

        std::unordered_map<ClientId, std::vector<std::unique_ptr<Agent>>> m_agents;
        std::unordered_map<AgentId, AgentEntry> m_agentIndex;
    };
}
