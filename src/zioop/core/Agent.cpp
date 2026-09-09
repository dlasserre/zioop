#include "zioop/core/Agent.hpp"

namespace core {

    namespace {
        std::size_t nextAgentId = 0;
    }

    Agent::Agent()
        : m_id(nextAgentId++) {
    }

}