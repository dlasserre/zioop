#include "zioop/core/Matrix.hpp"

#include <iostream>
#include <optional>
#include <stdexcept>

namespace core {

    Matrix::Matrix(
        const std::size_t sizeX,
        const std::size_t sizeY
    )
        : m_sizeX(sizeX),
          m_sizeY(sizeY),
          angles{{
              {0, 0},
              {sizeX - 1, 0},
              {0, sizeY - 1},
              {sizeX - 1, sizeY - 1}
          }}
    {
        if (sizeX == 0 || sizeY == 0) {
            throw std::invalid_argument(
                "Matrix dimensions must be greater than 0"
            );
        }

        m_data.resize(m_sizeX);

        for (std::size_t x = 0; x < m_sizeX; ++x) {

            m_data[x].resize(m_sizeY);

            for (std::size_t y = 0; y < m_sizeY; ++y) {

                m_data[x][y].pos = {
                    .x = x,
                    .y = y
                };
            }
        }
    }


    bool Matrix::caseIsFull(
        const Position& position
    ) const {

        if (
            position.x >= m_sizeX ||
            position.y >= m_sizeY
        ) {
            throw std::out_of_range(
                "Position outside matrix"
            );
        }

        return !m_data[position.x][position.y].empty();
    }

    std::optional<Position> Matrix::moveAgent(
        Agent& agent,
        Direction direction
    ) {

        const Case* cell = agent.getCell();

        if (!cell || !cell->agents.contains(agent.getId())) {
            return std::nullopt;
        }

        Position target = cell->pos;

        switch (direction) {
            case Direction::NORTH:
                target.x--;
                break;
            case Direction::SOUTH:
                target.y = (target.y + 1) % m_sizeY;
                break;
            case Direction::EAST:
                target.x = (target.x +1) % m_sizeX;
                break;
            case Direction::WEST:
                target.x = target.x == 0 ? m_sizeX - 1 : target.x - 1;
                break;
            default:
                return std::nullopt;
        }

        Case& destination = m_data[target.x][target.y];

        // Dans le cadre d'une matrice d'une case ???
        if (&destination == cell) {
            return target;
        }

        if (!destination.empty()) {
            return std::nullopt;
        }

        Case& source = m_data[cell->pos.x][cell->pos.y];

        destination.agents.emplace(agent.getId(), &agent);
        source.agents.erase(agent.getId());
        agent.setCase(&destination);

        std::cout
            << "Agent=" << agent.getId()
            << " from (" << cell->pos.x << ", " << cell->pos.y << ")"
            << " to (" << target.x << ", " << target.y << ")"
            << " direction (" << static_cast<std::size_t>(direction) << ")"
            << std::endl;

        return target;
    }


    std::optional<Position> Matrix::addAgent(
        Agent& agent,
        const ANGLE angle
    ) {

        const Position position = angles.at(angle);

        if (caseIsFull(position)) {
            return std::nullopt;
        }

        m_data[position.x][position.y]
            .agents
            .emplace(agent.getId(), &agent);

        m_agents.emplace(agent.getId(), &agent);
        agent.setCase(&m_data[position.x][position.y]);

        return position;
    }

}
