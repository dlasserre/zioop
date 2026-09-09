#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "zioop/core/Agent.hpp"

namespace core {

    enum ANGLE {
        TOP_LEFT = 0,
        TOP_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_RIGHT
    };

    struct Position {
        std::size_t x;
        std::size_t y;
    };

    struct Case {
        Position pos;
        std::vector<Agent*> agents;

        [[nodiscard]]
        bool empty() const {
            return agents.empty();
        }
    };

    class Matrix {
    public:
        explicit Matrix(std::size_t sizeX, std::size_t sizeY);

        bool addAgent(Agent& agent, ANGLE angle);

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

    private:
        std::size_t m_sizeX;
        std::size_t m_sizeY;

    public:
        const std::array<Position, 4> angles;

    private:
        std::vector<std::vector<Case>> m_data;
        std::vector<Agent*> m_agents;
    };

}