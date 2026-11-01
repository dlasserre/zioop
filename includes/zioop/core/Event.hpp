#pragma once

#include <ostream>
#include <variant>
#include <optional>
#include <string>
#include <cstddef>

#include "Types.hpp"

namespace core {

    struct BaseEvent {
        std::size_t agentId;
        std::size_t clientId;
    };

    struct Message {
        BaseEvent base;
        std::size_t speakerId;
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

    using Event = std::variant<Message, Died, RitualCompleted>;

}
