#pragma once

#include <cstddef>

namespace core {

    class Agent {
    public:
        Agent();

        [[nodiscard]]
        std::size_t getId() const {
            return m_id;
        }

    private:
        std::size_t m_id;
    };

}