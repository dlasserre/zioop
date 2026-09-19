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

        m_cells.resize(m_sizeX);

        std::uniform_int_distribution<int> quantity(0, 3);
        std::uniform_int_distribution<std::size_t> chooseType(
            0, types.size() - 1
        );

        for (std::size_t x = 0; x < m_sizeX; ++x) {
            m_cells[x].resize(m_sizeY);

            for (std::size_t y = 0; y < m_sizeY; ++y) {
                Cell &cell = m_cells[x][y];
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
        for (auto &column: m_cells) {
            for (auto &cell: column) {
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

        return !m_cells[position.x][position.y].empty();
    }

    const Cell *Matrix::getCell(const Position &position) const {
        if (position.x >= m_sizeX || position.y >= m_sizeY) {
            return nullptr;
        }

        return &m_cells[position.x][position.y];
    }

    std::optional<Position> Matrix::getNeighborPosition(
        Position target,
        Direction direction
    ) const {
        if (m_sizeX == 0 || m_sizeY == 0 ||
            target.x >= m_sizeX || target.y >= m_sizeY) {
            return std::nullopt;
        }

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

        return target;
    }

    std::vector<Position> Matrix::getVisiblePositions(
        const Position origin,
        const Direction direction,
        const int level
    ) const {
        Direction left;
        Direction right;

        switch (direction) {
            case Direction::NORTH:
                left = Direction::WEST;
                right = Direction::EAST;
                break;
            case Direction::SOUTH:
                left = Direction::EAST;
                right = Direction::WEST;
                break;
            case Direction::EAST:
                left = Direction::NORTH;
                right = Direction::SOUTH;
                break;
            case Direction::WEST:
                left = Direction::SOUTH;
                right = Direction::NORTH;
                break;
            default:
                return {};
        }

        if (level <= 0 || origin.x >= m_sizeX || origin.y >= m_sizeY) {
            return {};
        }

        std::vector<Position> visible;
        Position center = origin;

        for (int depth = 0; depth < level; ++depth) {
            if (depth > 0) {
                center = getNeighborPosition(center, direction).value();
            }

            Position current = center;

            for (int i = 0; i < depth; ++i) {
                current = getNeighborPosition(current, left).value();
            }

            visible.push_back(current);

            for (int i = 0; i < depth; ++i) {
                current = getNeighborPosition(current, right).value();
                visible.push_back(current);

                current = getNeighborPosition(current, right).value();
                visible.push_back(current);
            }
        }

        return visible;
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

        Cell &destination = m_cells[target.x][target.y];

        // Dans le cadre d'une matrice d'une seule case ???
        if (&destination == cell) {
            return target;
        }

        if (destination.isFull()) {
            return std::nullopt;
        }

        Cell &source = m_cells[cellPosition.x][cellPosition.y];

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
            Cell &source = m_cells[cellPosition.x][cellPosition.y];
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

        m_cells[position.x][position.y].emplace(agent);

        m_agents.emplace(agent.getId(), &agent);
        agent.setCell(&m_cells[position.x][position.y]);

        return position;
    }
}
