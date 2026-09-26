#pragma once

#include <vector>

#include "Resource.hpp"
#include "Types.hpp"

namespace core {

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
        std::vector<ObservedCell> cells;
    };

}
