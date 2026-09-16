#include "zioop/core/Cell.hpp"

#include <algorithm>
#include <memory>

#include "zioop/core/Fruit.hpp"

namespace core {

    bool Cell::emplace(Agent &agent) {
        for (Agent *&_agent: agents) {
            if (_agent == nullptr) {
                _agent = &agent;
                return true;
            }
        }

        return false;
    }

    bool Cell::isFull() const {
        return std::ranges::all_of(agents, [](const Agent *agent) {
            return agent != nullptr;
        });
    }

    bool Cell::empty() const {
        return std::ranges::all_of(agents, [](const Agent *agent) {
            return agent == nullptr;
        });
    }

    bool Cell::contains(const Agent &agent) {
        return std::ranges::any_of(agents, [&agent](Agent *&_agent) {
            return &agent == _agent;
        });
    }

    bool Cell::remove(const Agent &agent) {
        for (Agent *&_agent: agents) {
            if (_agent == &agent) {
                _agent = nullptr;
                return true;
            }
        }

        return false;
    }

    void Cell::addResource(ResourceType type) {
        auto resource = std::make_unique<Fruit>(type);
        m_resources.emplace_back(std::move(resource));
    }

    std::unique_ptr<Resource> Cell::take(ResourceType type) {
        auto it = std::ranges::find_if(
            m_resources,
            [type](const auto& item) {
                return item->getType() == type;
            }
        );

        if (it == m_resources.end()) {
            return nullptr;
        }

        auto resource = std::move(*it);
        m_resources.erase(it);
        return resource;
    }

    void Cell::updateResources() const {
        for (const auto& resource : m_resources) {
            resource->startRotting();
        }
    }

}
