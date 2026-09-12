#pragma once

#include <cstddef>
namespace core {

    struct Cell;

    class Agent {
    public:
        Agent();

        [[nodiscard]]
        std::size_t getId() const {
            return m_id;
        }

        const Cell* getCell() const {
            return m_cell;
        }

        void setCell(Cell* cell);

    private:
        std::size_t m_id;
        Cell* m_cell{nullptr};
    };

}
