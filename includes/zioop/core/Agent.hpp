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

    inline constexpr std::array<ResourceCount, 3> RITUAL_REQUIREMENTS{{
            {10, 5,  2, 1},
            {20, 10, 4, 2},
            {40, 15, 8, 3},
        }
    };

    class Cell;
    class Matrix;

    class Agent {
    public:
        enum LEVEL { BRONZY = 1, SILVER = 2, GOLD = 3 };

        explicit Agent(ClientId clientId);

        [[nodiscard]] AgentId getId() const { return m_id; }

        [[nodiscard]] int getLevel() const { return m_level; }

        [[nodiscard]] bool isRitualInProgress() const { return m_ritual.has_value(); }

        [[nodiscard]] bool startRitual(
            std::chrono::seconds duration,
            std::chrono::steady_clock::time_point now
        );

        [[nodiscard]] bool completeRitual(std::chrono::steady_clock::time_point now);

        [[nodiscard]] Inventory &getInventory();

        [[nodiscard]] std::vector<Inventory::InventoryResource> getItems() const;

        [[nodiscard]] Cell *getCell() const { return m_cell; }

        [[nodiscard]] std::optional<Position> move(
            Matrix &matrix,
            Direction direction
        );

        [[nodiscard]] std::optional<LookResult> look(
            const Matrix &matrix,
            Direction direction
        ) const;

        [[nodiscard]] AttackResult attack(Agent* target);

        [[nodiscard]] bool killed() const { return m_health == 0; }

        [[nodiscard]] int getHealth() const { return m_health; }

        [[nodiscard]] std::optional<std::string> getSpeech(std::chrono::steady_clock::time_point now) const;

        [[nodiscard]] std::optional<Direction> getLookDirection() const {
            return m_lookDirection;
        }

        [[nodiscard]] std::size_t getLookCount() const {
            return m_lookCount;
        }

        [[nodiscard]] std::optional<AgentId> getAttackTarget() const {
            return m_attackTarget;
        }

        [[nodiscard]] std::size_t getAttackCount() const {
            return m_attackCount;
        }

        [[nodiscard]] ClientId getClientId() const {
            return m_clientId;
        }

        bool take(ResourceType type);

        void setCell(Cell *cell);

        bool damage(int damage);

        void say(std::string message, std::chrono::steady_clock::time_point until);

        void recordLook(Direction direction);

        void setHealth(const int health) {
            if (health <= 0 || health > 100) {
                return;
            }
            m_health = health;
        }

    private:
        struct LevelResources {
            int level;
            std::unordered_map<ResourceType, std::size_t> resources;
        };

        struct Speech {
            std::string message;
            std::chrono::steady_clock::time_point until;
        };

        struct Ritual {
            int targetLevel;
            std::chrono::steady_clock::time_point finishesAt;
        };

        std::optional<AgentId> m_attackTarget;
        std::size_t m_attackCount{0};
        std::size_t m_lookCount{0};
        std::optional<Direction> m_lookDirection;
        std::optional<Speech> m_speech;

        ClientId m_clientId;
        AgentId m_id;

        Cell *m_cell{nullptr};
        int m_level{1};
        int m_health{FULL_LIFE};
        std::optional<Ritual> m_ritual;
        Inventory m_inventory;
    };
}
