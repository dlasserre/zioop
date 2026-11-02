#include "zioop/core/Matrix.hpp"

#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace core {
    static std::mt19937 generator(std::random_device{}());

    constexpr std::array types{
        ResourceType::APPLE,
        ResourceType::CAKE,
        ResourceType::MAGIC_WATER,
        ResourceType::MEAT
    };

    Matrix::Matrix(
        const std::size_t sizeX,
        const std::size_t sizeY
    )
        : m_sizeX(sizeX),
          m_sizeY(sizeY),
          angles{
              {
                  {.x = 0, .y = 0},
                  {.x = sizeX - 1, .y = 0},
                  {.x = 0, .y = sizeY - 1},
                  {.x = sizeX - 1, .y = sizeY - 1}
              }
          } {
        if (sizeX == 0 || sizeY == 0) {
            throw std::invalid_argument(
                "Matrix dimensions must be greater than 0"
            );
        }

        std::uniform_int_distribution quantity(0, 3);
        std::uniform_int_distribution<std::size_t> chooseType(0, types.size() - 1);

        m_cells.reserve(m_sizeX);

        for (std::size_t x = 0; x < m_sizeX; ++x) {
            std::vector<std::unique_ptr<Cell> > row;
            row.reserve(m_sizeY);

            for (std::size_t y = 0; y < m_sizeY; ++y) {
                auto cell = std::make_unique<Cell>();

                cell->setPosition({
                    .x = x,
                    .y = y
                });

                const int count = quantity(generator);
                for (int i = 0; i < count; ++i) {
                    cell->addResource(types[chooseType(generator)]);
                }

                row.push_back(std::move(cell));
            }

            m_cells.push_back(std::move(row));
        }
    }

    void Matrix::updateResources() const {
        for (auto &column: m_cells) {
            for (auto &cell: column) {
                cell->updateResources();
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

        return !m_cells[position.x][position.y]->empty();
    }

    const Cell *Matrix::getCell(const Position &position) const {
        if (position.x >= m_sizeX || position.y >= m_sizeY) {
            return nullptr;
        }

        return m_cells[position.x][position.y].get();
    }

    std::optional<Position> Matrix::offsetMatrix(
        Position origin,
        const Offset offset
    ) const {
        const auto x = static_cast<std::ptrdiff_t>(origin.x) + offset.x;
        const auto y = static_cast<std::ptrdiff_t>(origin.y) + offset.y;

        if (
            x < 0 || x >= static_cast<std::ptrdiff_t>(m_sizeX) ||
            y < 0 || y >= static_cast<std::ptrdiff_t>(m_sizeY)
        ) {
            return std::nullopt;
        }
        // cast to signed int64
        origin.x = static_cast<std::size_t>(x);
        origin.y = static_cast<std::size_t>(y);

        return origin;
    }


    std::optional<Position> Matrix::getNeighborPosition(
        const Position target,
        const Direction direction
    ) const {
        if (m_sizeX == 0 || m_sizeY == 0 ||
            target.x >= m_sizeX || target.y >= m_sizeY) {
            return std::nullopt;
        }
        const Offset offset = getOffset(direction);

        if (offset.x == 0 && offset.y == 0) {
            return std::nullopt;
        }

        return offsetMatrix(
            target,
            offset
        );
    }

    std::vector<Position> Matrix::getVisiblePositions(
        const Position origin,
        const Direction direction,
        const int level
    ) const {
        Direction right;

        switch (direction) {
            case Direction::NORTH:
                right = Direction::EAST;
                break;
            case Direction::SOUTH:
                right = Direction::WEST;
                break;
            case Direction::EAST:
                right = Direction::SOUTH;
                break;
            case Direction::WEST:
                right = Direction::NORTH;
                break;
            default:
                return {};
        }

        if (level <= 0 || origin.x >= m_sizeX || origin.y >= m_sizeY) {
            return {};
        }

        const Offset forward = getOffset(direction);
        const Offset lateral = getOffset(right);

        std::vector<Position> visible;

        /**
        * depth 2 :   (3,3) (4,3) (5,3) (6,3) (7,3)      left = -2 … +2
        * depth 1 :         (4,4) (5,4) (6,4)            left = -1 … +1
        * depth 0 :               (5,5)                  left = 0   ← l'alien
        **/
        for (int depth = 0; depth < level; ++depth) {
            for (int left = -depth; left <= depth; ++left) {
                const Offset offset{
                    .x = forward.x * depth + lateral.x * left,
                    .y = forward.y * depth + lateral.y * left
                };
                if (const auto position = offsetMatrix(origin, offset)) {
                    visible.push_back(*position);
                }
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
        auto [x, y] = cell->getPosition();

        const auto target = getNeighborPosition(cell->getPosition(), direction);
        if (!target) {
            return std::nullopt;
        }

        Cell &destination = *m_cells[target->x][target->y];

        if (&destination == cell) {
            return target;
        }

        if (destination.isFull()) {
            return std::nullopt;
        }

        Cell &source = *m_cells[x][y];

        destination.emplace(agent);
        source.remove(agent);
        agent.setCell(&destination);

        std::cout
                << "Agent=" << agent.getId()
                << " from (" << x << ", " << y << ")"
                << " to (" << target->x << ", " << target->y << ")"
                << " direction (" << static_cast<std::size_t>(direction) << ")"
                << std::endl;

        return target;
    }

    Cell *Matrix::getFreeCell() const {
        Cell *selected = nullptr;
        int freeCount = 0;
        std::random_device rd;
        std::mt19937 gen(rd());

        for (const auto &row: m_cells) {
            for (const auto &cell: row) {
                if (!cell->isFull()) {
                    ++freeCount;
                    std::uniform_int_distribution distrib(1, freeCount);
                    if (distrib(gen) == 1) {
                        selected = cell.get();
                    }
                }
            }
        }

        return selected;
    }

    void Matrix::removeAgent(Agent &agent) const {
        if (const Cell *cell = agent.getCell()) {
            const auto &[x, y] = cell->getPosition();
            Cell &source = *m_cells[x][y];
            source.remove(agent);
        }
        agent.setCell(nullptr);
    }

    std::optional<Position> Matrix::addAgent(
        Agent &agent
    ) const {
        const auto cell = getFreeCell();
        if (!cell) {
            return std::nullopt;
        }
        cell->emplace(agent);

        return cell->getPosition();
    }
}
