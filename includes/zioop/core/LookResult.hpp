#pragma once

#include <vector>

#include "MatrixSnapshot.hpp"
#include "Resource.hpp"
#include "Types.hpp"

namespace core {

    enum class RitualStartResult {
        STARTED,
        ALREADY_IN_PROGRESS,
        NOT_ENOUGH_RESOURCES
    };

    enum class AttackResult {
        KILLED,
        RITUAL_IN_PROGRESS,
        TARGET_NOT_FOUND,
        SAME_OWNER,
        TOO_FAR,
        TARGET_TOO_STRONG,
        DAMAGED
    };

    struct DefaultResult {
        std::size_t agentId;
    } typedef TakeResult;

    struct ObservedResource {
        ResourceType type;
        int rotting;
    };

    struct ObservedAgent {
        std::size_t id;
        int level;
    };

    struct ObservedCell {
        Position position;
        std::vector<ObservedResource> resources;
        std::vector<ObservedAgent> agents;
    };

    struct LookResult {
        DefaultResult base;
        std::vector<ObservedCell> cells;
    };

    struct MoveResult {
        DefaultResult base;
        std::optional<Position> position;
    };

    struct HelloResult {
        DefaultResult base;
        Position position;
    };

    struct MatrixResult {
        MatrixSnapshot snapshot;
    };

    struct InventoryResult {
        DefaultResult base;
        std::vector<InventoryResource> inventory;
    };

    struct RitualResult {
        DefaultResult base;
        RitualStartResult result;
    };
}
