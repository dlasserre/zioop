#pragma once

#include <cstddef>
namespace core {

    struct Case;

    class Agent {
    public:
        Agent();

        [[nodiscard]]
        std::size_t getId() const {
            return m_id;
        }

        const Case* getCell() const {
            return m_cell;
        }

        void setCase(Case* cell);

    private:
        std::size_t m_id;
        Case* m_cell{nullptr};
    };

}
