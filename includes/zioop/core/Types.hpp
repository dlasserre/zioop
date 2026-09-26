#pragma once

#include <cstddef>
#include <cstdint>

namespace core {
    struct Position {
        std::size_t x;
        std::size_t y;
    };

    enum class Direction : std::uint16_t {
        NORTH = 0,
        SOUTH = 90,
        EAST = 180,
        WEST = 270,
    };
}
