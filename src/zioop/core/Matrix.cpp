#include "zioop/core/Matrix.hpp"

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


    std::optional<Position> Matrix::addAgent(
        Agent& agent,
        const ANGLE angle
    ) {

        const Position position = angles.at(angle);

        if (caseIsFull(position)) {
            return std::nullopt;
        }

        // On stocke l'adresse de l'agent
        m_data[position.x][position.y]
            .agents
            .push_back(&agent);

        m_agents.push_back(&agent);

        return position;
    }

}