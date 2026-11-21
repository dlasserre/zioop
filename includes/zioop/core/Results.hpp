#pragma once

#include <vector>

#include "zioop/core/MatrixSnapshot.hpp"


namespace core {

    struct InventoryResource;

    enum class RitualStartResult {
        STARTED,
        ALREADY_IN_PROGRESS,
        NOT_ENOUGH_RESOURCES
    };

    enum class AttackStatus {
        KILLED,
        RITUAL_IN_PROGRESS,
        TARGET_NOT_FOUND,
        SAME_OWNER,
        TOO_FAR,
        TARGET_TOO_STRONG,
        DAMAGED
    };

    constexpr std::string_view attackStatusErrorMessage(const AttackStatus status) {
        switch (status) {
            case AttackStatus::RITUAL_IN_PROGRESS:
                return "Ritual already in progress";
            case AttackStatus::SAME_OWNER:
                return "Cannot attack your own agent";
            case AttackStatus::TARGET_NOT_FOUND:
                return "Target not found";
            case AttackStatus::TARGET_TOO_STRONG:
                return "Target is too strong";
            case AttackStatus::TOO_FAR:
                return "Target is too far";
        }

        return "Unknown attack result";
    }

    enum class GiveStatus {
        GIVEN,
        RESOURCE_NOT_FOUND,
        TARGET_NOT_FOUND,
        CANT_GIVE_YOURSELF,
        TARGET_TO_FAR,
        FAILED
    };

    struct DefaultResult {
        AgentId agentId;
    } typedef TakeResult;

    struct AttackResult {
        DefaultResult base;
        AgentId targetId;
        AttackStatus status;
    };

    struct ErrorResult {
        DefaultResult base;
        int errorCode;
        std::string message;
    };

    struct GiveResult {
        DefaultResult base;
        AgentId targetId;
        GiveStatus status;
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

    struct HealResult {
        DefaultResult base;
        bool success{false};
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
        std::vector<InventoryResource> inventory;
    };

    struct TakenResult {
        DefaultResult base;
        std::vector<InventoryResource> inventory;
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
        GiveStatus status;
    };
}
