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
        static constexpr std::size_t RitualFruitCost = 10;
        static constexpr std::chrono::seconds RitualTimePerLevel{30};

        struct AgentPlacement {
            std::size_t agentId;
            Position position;
        };

        enum class AttackResult {
            KILLED,
            RITUAL_IN_PROGRESS,
            TARGET_NOT_FOUND,
            SAME_OWNER,
            TOO_FAR,
            TARGET_TOO_STRONG
        };

        struct TakeResult {
            bool taken;
            std::vector<InventoryResource> inventory;
        };

        enum class RitualStartResult {
            STARTED,
            ALREADY_IN_PROGRESS,
            NOT_ENOUGH_RESOURCES
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
        std::optional<AgentPlacement> addAgent(
            std::size_t clientId,
            ANGLE angle
        );

        [[nodiscard]]
        std::optional<Position> move(
            std::size_t clientId,
            std::size_t agentId,
            Direction direction
        );

        [[nodiscard]]
        std::optional<LookResult> look(
            std::size_t clientId,
            std::size_t agentId,
            Direction direction
        ) const;

        [[nodiscard]]
        std::optional<TakeResult> take(
            std::size_t clientId,
            std::size_t agentId,
            ResourceType type
        );

        [[nodiscard]]
        std::optional<std::vector<InventoryResource> > inventory(
            std::size_t clientId,
            std::size_t agentId
        );

        [[nodiscard]]
        std::optional<RitualStartResult> ritual(
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
        struct AgentEntry {
            std::size_t clientId;
            std::unique_ptr<Agent> agent;
        };

        mutable std::mutex m_stateMutex;
        Matrix m_matrix;
        std::unordered_map<std::size_t, AgentEntry> m_agents;
    };
}
