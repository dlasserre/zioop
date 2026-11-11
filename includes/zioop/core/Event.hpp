#pragma once

#include <ostream>
#include <variant>
#include <optional>
#include <string>
#include <cstddef>

#include "Resource.hpp"
#include "Types.hpp"

namespace core {

    struct BaseEvent {
        AgentId agentId;
        ClientId clientId;
    };

    struct Message {
        BaseEvent base;
        AgentId speakerId;
        std::optional<Direction> direction;
        std::string message;
    };

    struct Died {
        BaseEvent base;
    };

    struct RitualCompleted {
        BaseEvent base;
        int level;
    };

    struct DeletedRottenResources {
        BaseEvent base;
        std::vector<ResourceType> resources;
    };

    using Event = std::variant<Message, Died, RitualCompleted, DeletedRottenResources>;

}
