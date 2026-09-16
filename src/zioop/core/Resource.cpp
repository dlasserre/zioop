#include "zioop/core/Resource.hpp"

#include <chrono>
#include <mutex>

namespace core {
    Resource::Resource(ResourceType type)
        : m_type(type),
          m_nextDecay(
              std::chrono::steady_clock::now() + m_decayInterval
          ) {
    }

    void Resource::startRotting() {
        const auto now = std::chrono::steady_clock::now();

        while (now >= m_nextDecay && m_rotting.load() < 100) {
            decaying();
            m_nextDecay += m_decayInterval;
        }
    }
}
