#include "zioop/core/Resource.hpp"

#include <chrono>
#include <mutex>

namespace core {

    Resource::Resource(
        const ResourceType type)
            : m_type(type) {}

    void Resource::startRotting(const Clock::time_point now) {
        m_expiresAt = now + def(m_type).lifetime;
    }
}
