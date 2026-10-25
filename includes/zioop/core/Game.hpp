#pragma once

#include <chrono>
#include <mutex>
#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>

#include "Inventory.hpp"
#include "zioop/core/Matrix.hpp"
#include "zioop/core/Agent.hpp"
#include "zioop/core/LookResult.hpp"
#include "zioop/core/MatrixSnapshot.hpp"

namespace core {
    class Game {
    public:
        static constexpr std::chrono::seconds RitualTimePerLevel{30};

        struct InitPlacement {
            std::size_t agentId;
            Position position;
        };

        struct TakeResult {
            bool taken;
            std::vector<InventoryResource> inventory;
        };

        struct RitualCompleted {
            std::size_t clientId;
            std::size_t agentId;
            int level;
        };

        explicit Game(Matrix matrix);

        [[nodiscard]]
        std::vector<RitualCompleted> tick(
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()
        );

        [[nodiscard]]
        std::optional<InitPlacement> initClient(
            std::size_t clientId
        );

        std::optional<InitPlacement> createAgent(
            std::size_t clientId
        );

        [[nodiscard]]
        std::optional<MoveResult> move(
            std::size_t clientId,
            std::size_t agentId,
            Direction direction
        );

        [[nodiscard]]
        std::optional<LookResult> look(
            std::size_t clientId,
            std::size_t agentId,
            Direction direction
        );

        [[nodiscard]]
        std::optional<TakeResult> take(
            std::size_t clientId,
            std::size_t agentId,
            ResourceType type
        );

        [[nodiscard]]
        std::optional<InventoryResult> inventory(
            std::size_t clientId,
            std::size_t agentId
        );

        [[nodiscard]]
        std::optional<RitualResult> ritual(
            std::size_t clientId,
            std::size_t agentId
        );

        [[nodiscard]]
        std::optional<AttackResult> attack(
            std::size_t clientId,
            std::size_t agentId,
            std::size_t targetId
        );

        MatrixSnapshot getMatrix() const;

        void removeClientAgent(std::size_t clientId);

    private:

        Agent* findAgentUnlocked(std::size_t clientId, std::size_t agentId);
        Agent* findAgentUnlocked(std::size_t agentId);

        template <typename F>
        auto withAgent(std::size_t clientId, std::size_t agentId, F&& f);

        std::optional<InitPlacement> placeAgent(std::size_t clientId);

        std::optional<InitPlacement> placeAgent(std::vector<std::unique_ptr<Agent>>& agents);

        mutable std::mutex m_stateMutex;
        Matrix m_matrix;
        // ClientId
        std::unordered_map<std::size_t, std::vector<std::unique_ptr<Agent>>> m_agents;
    };
}
