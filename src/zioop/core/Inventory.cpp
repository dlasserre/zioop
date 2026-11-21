#include <algorithm>
#include <unordered_map>

#include "zioop/core/Inventory.hpp"
#include "zioop/core/Game.hpp"

namespace core {

    void Inventory::add(Resource resource, const Clock::time_point now) {
        resource.startRotting(now);
        m_resources.push_back(resource);
    }

    std::size_t Inventory::count(ResourceType type) const {
        return std::ranges::count_if(m_resources, [&](const auto& r) {
            return r.type() == type;
        });
    }

    bool Inventory::remove(const ResourceCount &required) {
        if (!hasAll(required)) {
            return false;
        }

        auto left = required;
        std::erase_if(m_resources, [&](const auto& resource) {
            auto &remaining = left[static_cast<std::size_t>(resource.type())];
            if (remaining == 0) {
                return false;
            }
            --remaining;
            return true;
        });
        return true;
    }

    bool Inventory::remove(const ResourceType type, const std::size_t quantity) {
        if (count(type) < quantity) {
            return false;
        }

        std::size_t left = quantity;
        std::erase_if(m_resources, [&](const auto &r) {
            if (left == 0 || type != r.type()) return false;
            --left;
            return true;
        });

        return true;
    }

    bool Inventory::has(const ResourceType type, const std::size_t quantity) const {
        return count(type) >= quantity;
    }

    bool Inventory::has(const ResourceType type) const {
        return has(type, 1);
    }

    std::unordered_map<ResourceType, std::size_t> Inventory::getItems() const {
        std::unordered_map<ResourceType, std::size_t> items;

        for (const auto& r : m_resources) {
            ++items[r.type()];
        }

        return items;
    }

    bool Inventory::hasAll(const ResourceCount &required) const {
        for (std::size_t type = 0; type < RESOURCE_KIND; type++) {
            if (count(static_cast<ResourceType>(type)) < required[type]) {
                return false;
            }
        }

        return true;
    }

    std::vector<ResourceType> Inventory::update(const Clock::time_point now) {
        std::vector<ResourceType> rotted;

        std::erase_if(m_resources, [&](auto& r) {
            if (!r.isRotten(now))
                return false;
            rotted.push_back(r.type());
            return true;
        });

        return rotted;
    }

    void Inventory::received(const Resource resource) {
        m_resources.push_back(resource);
    }

    GiveStatus Inventory::give(ResourceType type, Inventory &target) {
        const auto it = std::ranges::find_if(m_resources, [&](const auto& r) {
            return r.type() == type;
        });

        if (it == m_resources.end()) {
            return GiveStatus::RESOURCE_NOT_FOUND;
        }

        target.received(*it);
        m_resources.erase(it);
        return GiveStatus::GIVEN;
    }

}
