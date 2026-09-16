#include "zioop/core/Matrix.hpp"

#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>


namespace core {
    static std::mt19937 generator(std::random_device{}());

    constexpr std::array types{
        ResourceType::FRUIT
    };

    Matrix::Matrix(
        const std::size_t sizeX,
        const std::size_t sizeY
    )
        : m_sizeX(sizeX),
          m_sizeY(sizeY),
          angles{
              {
                  {0, 0},
                  {sizeX - 1, 0},
                  {0, sizeY - 1},
                  {sizeX - 1, sizeY - 1}
              }
          } {
        if (sizeX == 0 || sizeY == 0) {
            throw std::invalid_argument(
                "Matrix dimensions must be greater than 0"
            );
        }

        m_data.resize(m_sizeX);

        std::uniform_int_distribution<int> quantity(0, 3);
        std::uniform_int_distribution<std::size_t> chooseType(
            0, types.size() - 1
        );

        for (std::size_t x = 0; x < m_sizeX; ++x) {
            m_data[x].resize(m_sizeY);

            for (std::size_t y = 0; y < m_sizeY; ++y) {

                Cell& cell = m_data[x][y];
                cell.setPosition({
                    .x = x,
                    .y = y
                });
                const int count = quantity(generator);
                for (int i = 0; i < count; ++i) {
                    cell.addResource(types[chooseType(generator)]);
                }
            }
        }
    }

    void Matrix::updateResources() const {
        for (auto& column : m_data) {
            for (auto& cell : column) {
                cell.updateResources();
            }
        }
    }


    bool Matrix::caseIsFull(
        const Position &position
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
        Agent &agent,
        Direction direction
    ) {
        Cell *cell = agent.getCell();

        if (!cell || !cell->contains(agent)) {
            return std::nullopt;
        }
        Position cellPosition = cell->getPosition();
        Position target = cell->getPosition();

        switch (direction) {
            case Direction::NORTH:
                target.y = target.y == 0 ? m_sizeY - 1 : target.y - 1;
                break;
            case Direction::SOUTH:
                target.y = (target.y + 1) % m_sizeY;
                break;
            case Direction::EAST:
                target.x = (target.x + 1) % m_sizeX;
                break;
            case Direction::WEST:
                target.x = target.x == 0 ? m_sizeX - 1 : target.x - 1;
                break;
            default:
                return std::nullopt;
        }

        Cell &destination = m_data[target.x][target.y];

        // Dans le cadre d'une matrice d'une seule case ???
        if (&destination == cell) {
            return target;
        }

        if (destination.isFull()) {
            return std::nullopt;
        }

        Cell &source = m_data[cellPosition.x][cellPosition.y];

        destination.emplace(agent);
        source.remove(agent);
        agent.setCell(&destination);

        std::cout
                << "Agent=" << agent.getId()
                << " from (" << cellPosition.x << ", " << cellPosition.y << ")"
                << " to (" << target.x << ", " << target.y << ")"
                << " direction (" << static_cast<std::size_t>(direction) << ")"
                << std::endl;

        return target;
    }

    void Matrix::removeAgent(Agent &agent) {
        if (const Cell *cell = agent.getCell()) {
            Position cellPosition = cell->getPosition();
            Cell &source = m_data[cellPosition.x][cellPosition.y];
            source.remove(agent);
        }
        m_agents.erase(agent.getId());
        agent.setCell(nullptr);
    }

    std::optional<Position> Matrix::addAgent(
        Agent &agent,
        const ANGLE angle
    ) {
        const Position position = angles.at(angle);

        if (caseIsFull(position)) {
            return std::nullopt;
        }

        m_data[position.x][position.y].emplace(agent);

        m_agents.emplace(agent.getId(), &agent);
        agent.setCell(&m_data[position.x][position.y]);

        return position;
    }
}
