#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

#include "zioop/core/Agent.hpp"

namespace core {

    enum ANGLE {
        TOP_LEFT = 0,
        TOP_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_RIGHT
    };

    enum class Direction : std::uint16_t {
        NORTH = 0,
        SOUTH = 90,
        EAST = 180,
        WEST = 270,
    };

    struct Position {
        std::size_t x;
        std::size_t y;
    };

    struct Case {
        Position pos;
        std::unordered_map<std::size_t, Agent*> agents;

        [[nodiscard]]
        bool empty() const {
            return agents.empty();
        }
    };

    class Matrix {
    public:
        explicit Matrix(std::size_t sizeX, std::size_t sizeY);

        std::optional<Position> addAgent(Agent& agent, ANGLE angle);

        std::optional<Position> moveAgent(Agent& agent, Direction direction);

        [[nodiscard]]
        bool caseIsFull(const Position& position) const;

        [[nodiscard]]
        const std::vector<std::vector<Case>>& getMatrix() const {
            return m_data;
        }

        [[nodiscard]]
        std::size_t getTotalAgents() const {
            return m_agents.size();
        }

        [[nodiscard]]
        std::size_t getSizeX() const {
            return m_sizeX;
        }

        [[nodiscard]]
        std::size_t getSizeY() const {
            return m_sizeY;
        }

    private:
        std::size_t m_sizeX;
        std::size_t m_sizeY;

    public:
        const std::array<Position, 4> angles;

    private:
        std::vector<std::vector<Case>> m_data;

        std::unordered_map<std::size_t, Agent*> m_agents;

    };

}