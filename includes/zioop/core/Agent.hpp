#pragma once

#include <chrono>
#include <optional>
#include <unordered_map>
#include <array>

#include "Inventory.hpp"
#include "zioop/core/Types.hpp"
#include "zioop/core/LookResult.hpp"

namespace core {
    class Cell;
    class Matrix;

    class Agent {
    public:
        //enum class ResourceType { FRUIT, WOOD, STONE };
        enum LEVEL { BRONZY, SILVER, GOLD };

        Agent();

        [[nodiscard]]
        std::size_t getId() const {
            return m_id;
        }

        [[nodiscard]]
        int getLevel() const {
            return m_level;
        }

        [[nodiscard]]
        bool isRitualInProgress() const {
            return m_ritual.has_value();
        }

        [[nodiscard]]
        bool startRitual(
            std::chrono::seconds duration,
            std::chrono::steady_clock::time_point now
        );

        [[nodiscard]]
        bool completeRitual(std::chrono::steady_clock::time_point now);

        [[nodiscard]]
        std::vector<InventoryResource> getInventory() const;

        void updateInventory() const;

        [[nodiscard]]
        Cell *getCell() const {
            return m_cell;
        }

        [[nodiscard]]
        std::optional<Position> move(
            Matrix &matrix,
            Direction direction
        );

        [[nodiscard]]
        std::optional<LookResult> look(
            const Matrix &matrix,
            Direction direction
        ) const;

        bool take(ResourceType type);

        void setCell(Cell *cell);

        bool damage(int damage);

        [[nodiscard]]
        bool killed() const {
            return m_health == 0;
        }

        [[nodiscard]]
        int getHealth() const {
            return m_health;
        }

    private:

        bool consumeRequiredResourcesForLevel();

        struct LevelResources {
            int level;
            std::unordered_map<ResourceType, std::size_t> resources;
        };

        const std::vector<LevelResources> m_requiredResources = {
            {
                {
                    .level = BRONZY,
                    .resources = {
                        {ResourceType::APPLE, 10},
                        {ResourceType::MEAT, 2},
                    }
                },
                {
                    .level = SILVER,
                    .resources = {
                        {ResourceType::APPLE, 20},
                        {ResourceType::MEAT, 4},
                    }
                },
                {
                    .level = GOLD,
                    .resources = {
                        {ResourceType::APPLE, 50},
                        {ResourceType::MEAT, 10},
                        {ResourceType::CAKE, 8},
                    }
                },
            }
        };

        struct Ritual {
            int targetLevel;
            std::chrono::steady_clock::time_point finishesAt;
        };

        bool m_alive{true};

        std::size_t m_id;

        Cell *m_cell{nullptr};

        int m_level{1};

        int m_health{100};

        std::optional<Ritual> m_ritual;

        std::vector<std::unique_ptr<Resource> > m_inventory;
    };
}
