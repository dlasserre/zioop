#include "zioop/core/Agent.hpp"

namespace core {

    namespace {
        std::size_t nextAgentId = 1;
    }

    Agent::Agent()
        : m_id(nextAgentId++) {
    }

    void Agent::setCell(Cell* cell) {
        m_cell = cell;
    }

}
