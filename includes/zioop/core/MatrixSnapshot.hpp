#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "zioop/core/Types.hpp"
#include "zioop/core/Resource.hpp"

namespace core {
    struct ResourceSnapshot {
        Position position;
        std::size_t subCell;
        ResourceType type;
        int rotting;
    };

    struct ItemInventory {
        ResourceType type;
        std::size_t quantity;
    };

    struct AgentSnapshot {
        std::size_t agentId;
        int health;
        int level;
        Position position;
        std::size_t subCell;
        std::vector<ItemInventory> inventory;
        std::optional<std::string> message;
        bool ritual{false};
        std::optional<Direction> lookDirection;
        std::size_t lookCount{0};
    };

    struct MatrixSnapshot {
        std::size_t sizeX{};
        std::size_t sizeY{};
        std::vector<ResourceSnapshot> resources;
        std::vector<AgentSnapshot> agents;
    };
}
