#pragma once

#include <chrono>
#include <mutex>
#include <queue>
#include <memory>
#include <optional>
#include <unordered_map>
#include <array>
#include <cstddef>

#include "Event.hpp"
#include "Inventory.hpp"
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

    class Game {
    public:
        static constexpr std::chrono::seconds RitualTimePerLevel{30};

        struct InitPlacement {
            AgentId agentId;
            Position position;
        };

        struct TakeResult {
            bool taken;
            std::vector<Inventory::InventoryResource> inventory;
        };

        std::vector<Event> drainEvents();

        explicit Game(Matrix matrix);

        void tick(std::chrono::steady_clock::time_point now);

        [[nodiscard]]
        std::optional<InitPlacement> initClient(
            ClientId clientId
        );

        std::optional<InvocationResult> instanciatAgent(
            ClientId clientId,
            AgentId agentId
        );

        [[nodiscard]]
        std::optional<MoveResult> move(
            ClientId clientId,
            AgentId agentId,
            Direction direction
        );

        [[nodiscard]]
        std::optional<BroadcastResult> broadcast(
            ClientId clientId,
            AgentId agentId,
            std::string &message
        );

        [[nodiscard]]
        std::optional<LookResult> look(
            ClientId clientId,
            AgentId agentId,
            Direction direction
        );

        [[nodiscard]]
        std::optional<TakeResult> take(
            ClientId clientId,
            AgentId agentId,
            ResourceType type
        );

        [[nodiscard]]
        std::optional<GiveResult> give(
            ClientId clientId,
            AgentId agentId,
            ResourceType type,
            AgentId targetId
        );

        [[nodiscard]]
        std::optional<InventoryResult> inventory(
            ClientId clientId,
            AgentId agentId
        );

        [[nodiscard]]
        std::optional<RitualResult> ritual(
            ClientId clientId,
            AgentId agentId
        );

        [[nodiscard]]
        std::optional<AttackResult> attack(
            ClientId clientId,
            AgentId agentId,
            AgentId targetId
        );

        MatrixSnapshot getMatrix() const;

        void removeClientAgent(ClientId clientId);

        [[nodiscard]] std::size_t countMonsters(ClientId clientId);
    private:

        int  m_test{0};



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
    };
}
