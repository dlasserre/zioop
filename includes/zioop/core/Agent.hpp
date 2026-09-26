#pragma once

#include <chrono>
#include <optional>

#include "Inventory.hpp"
#include "zioop/core/Types.hpp"
#include "zioop/core/LookResult.hpp"

namespace core {
    class Cell;
    class Matrix;

    class Agent {
    public:
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
            std::size_t fruitCost,
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
            const Matrix& matrix,
            Direction direction
        ) const;

        bool take(ResourceType type);

        void setCell(Cell *cell);

        void kill();

    private:
        struct Ritual {
            int targetLevel;
            std::chrono::steady_clock::time_point finishesAt;
        };

        bool m_alive{true};

        std::size_t m_id;

        Cell *m_cell{nullptr};

        int m_level{1};

        std::optional<Ritual> m_ritual;

        std::vector<std::unique_ptr<Resource>> m_inventory;
    };
}
