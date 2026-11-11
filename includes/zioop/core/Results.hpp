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

    enum class GiveResult {
        GIVED,
        RESOURCE_NOT_FOUND,
        TARGET_NOT_FOUND,
        CANT_GIVE_YOURSELF,
        TARGET_TO_FAR
    };

    struct DefaultResult {
        AgentId agentId;
    } typedef TakeResult;

    struct ErrorResult {
        DefaultResult base;
        std::string message;
    };

    struct InvocationResult {
        DefaultResult base;
        AgentId spawnedAgentId;
        std::optional<Position> position;
    };

    struct ObservedResource {
        ResourceType type;
    };

    struct ObservedAgent {
        AgentId id;
        int level;
        int health;
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

    struct BroadcastResult {
        DefaultResult base;
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
        std::vector<Inventory::InventoryResource> inventory;
    };

    struct RitualResult {
        DefaultResult base;
        RitualStartResult result;
    };

    struct AttackResponse {
        DefaultResult base;
        AgentId targetId;
        bool killed;
    };

    struct GiveResponse {
        DefaultResult base;
        ResourceType resource;
        AgentId targetAgentId;
    };
}
