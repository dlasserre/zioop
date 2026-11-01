#pragma once

#include <chrono>
#include <optional>
#include <unordered_map>
#include <string>

#include "Event.hpp"
#include "Inventory.hpp"
#include "zioop/core/Types.hpp"
#include "zioop/core/Results.hpp"

namespace core {
    constexpr int FULL_LIFE = 100;

    class Cell;
    class Matrix;

    class Agent {
    public:
        enum LEVEL { BRONZY = 1, SILVER = 2, GOLD = 3 };

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

        std::vector<Event> update(std::chrono::steady_clock::time_point now);

        void say(std::string message, std::chrono::steady_clock::time_point until);

        [[nodiscard]]
        std::optional<std::string> getSpeech(std::chrono::steady_clock::time_point now) const;

        void recordLook(Direction direction);

        [[nodiscard]]
        std::optional<Direction> getLookDirection() const {
            return m_lookDirection;
        }

        void setHealth(const int health) {
            if (health <= 0 || health > 100) {
                return;
            }
            m_health = health;
        }

        [[nodiscard]]
        std::size_t getLookCount() const {
            return m_lookCount;
        }

        [[nodiscard]]
        bool isAlive() const {
            return m_alive;
        }

    private:

        std::optional<Direction> m_lookDirection;

        std::size_t m_lookCount{0};

        struct Speech {
            std::string message;
            std::chrono::steady_clock::time_point until;
        };

        std::optional<Speech> m_speech;

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

        int m_health{FULL_LIFE};

        std::optional<Ritual> m_ritual;

        std::vector<std::unique_ptr<Resource> > m_inventory;
    };
}
