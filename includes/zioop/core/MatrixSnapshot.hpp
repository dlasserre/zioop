#pragma once

#include <cstddef>
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

    struct AgentSnapshot {
        std::size_t agentId;
        bool alive;
        Position position;
        std::size_t subCell;
    };

    struct MatrixSnapshot {
        std::size_t sizeX{};
        std::size_t sizeY{};
        std::vector<ResourceSnapshot> resources;
        std::vector<AgentSnapshot> agents;
    };
}
