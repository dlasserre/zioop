#pragma once

#include <vector>
#include <unordered_map>

#include "zioop/core/Resource.hpp"
#include "zioop/core/Results.hpp"

namespace core {

    struct InventoryResource {
        ResourceType type;
        std::size_t quantity;
    };

    class Inventory {
    public:
        using Clock = std::chrono::steady_clock;

        struct Item {
            std::size_t count{0};
        };

        void add(Resource resource, Clock::time_point now);

        bool remove(ResourceType type, std::size_t quantity);

        bool remove(const ResourceCount &required);

        void received(Resource resource);

        [[nodiscard]] GiveStatus give(ResourceType type, Inventory &target);

        [[nodiscard]] std::vector<ResourceType> update(Clock::time_point now);

        [[nodiscard]] std::size_t count(ResourceType type) const;

        [[nodiscard]] bool hasAll(const ResourceCount &required) const;

        [[nodiscard]] bool has(ResourceType type, std::size_t quantity) const;

        [[nodiscard]] bool has(ResourceType type) const;

        [[nodiscard]] std::unordered_map<ResourceType, std::size_t> getItems() const;

    private:
        std::vector<Resource> m_resources;
    };
}
