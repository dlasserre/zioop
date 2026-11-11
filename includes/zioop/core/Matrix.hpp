#pragma once

#include "zioop/core/Cell.hpp"
#include "zioop/core/Agent.hpp"
#include "zioop/core/Types.hpp"

#include <array>
#include <optional>
#include <vector>

namespace core {
    enum ANGLE {
        TOP_LEFT = 0,
        TOP_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_RIGHT
    };

    struct Offset {
        int x;
        int y;
    };

    constexpr Offset getOffset(const Direction direction) {
        switch (direction) {
            case Direction::NORTH: return {.x = 0, .y = -1};
            case Direction::SOUTH: return {.x = 0, .y = 1};
            case Direction::EAST: return {.x = 1, .y = 0};
            case Direction::WEST: return {.x = -1, .y = 0};
        }
        return {.x = 0, .y = 0};
    }

    class Matrix {
    public:
        using Clock = std::chrono::steady_clock;

        explicit Matrix(std::size_t sizeX, std::size_t sizeY);

        std::optional<Position> addAgent(Agent &agent);

        void removeAgent(Agent &agent);

        [[nodiscard]]
        std::optional<Position> getNeighborPosition(
            Position target,
            Direction direction
        ) const;

        [[nodiscard]] std::vector<Position> getVisiblePositions(
            Position origin,
            Direction direction,
            int level
        ) const;

        [[nodiscard]]
        const Cell* getCell(const Position &position) const;

        [[nodiscard]]
        Cell* getFreeCell() const;

        std::optional<Position> moveAgent(Agent &agent, Direction direction);

        [[nodiscard]]
        bool caseIsFull(const Position &position) const;

        [[nodiscard]]
        const std::vector<std::vector<std::unique_ptr<Cell>>> &getMatrix() const {
            return m_cells;
        }

        [[nodiscard]]
        std::size_t getSizeX() const {
            return m_sizeX;
        }

        [[nodiscard]]
        std::size_t getSizeY() const {
            return m_sizeY;
        }

        [[nodiscard]]
        std::optional<Position> offsetMatrix(
            Position origin,
            Offset offset
        ) const;

        void repop(Clock::time_point now);

    private:
        std::size_t m_sizeX;
        std::size_t m_sizeY;
        std::vector<std::vector<std::unique_ptr<Cell>>> m_cells;
        const std::array<Position, 4> angles;
    };
}
