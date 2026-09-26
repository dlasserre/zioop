#include <utility>

#include "zioop/core/Agent.hpp"

#include <algorithm>
#include <unordered_map>

#include "zioop/core/Inventory.hpp"
#include "zioop/core/Matrix.hpp"

namespace core {
    namespace {
        std::size_t nextAgentId = 1;
    }

    Agent::Agent()
        : m_id(nextAgentId++) {
    }

    bool Agent::startRitual(
        const std::size_t fruitCost,
        const std::chrono::seconds duration,
        const std::chrono::steady_clock::time_point now
    ) {
        if (isRitualInProgress()) {
            return false;
        }

        const auto fruits = std::ranges::count_if(m_inventory, [](const auto &resource) {
            return resource->getType() == ResourceType::FRUIT;
        });
        if (static_cast<std::size_t>(fruits) < fruitCost) {
            return false;
        }

        auto remaining = fruitCost;
        std::erase_if(m_inventory, [&remaining](const auto &resource) {
            if (remaining == 0 || resource->getType() != ResourceType::FRUIT) {
                return false;
            }
            --remaining;
            return true;
        });

        m_ritual = Ritual{
            .targetLevel = m_level + 1,
            .finishesAt = now + duration
        };
        return true;
    }

    bool Agent::completeRitual(const std::chrono::steady_clock::time_point now) {
        if (!m_ritual || now < m_ritual->finishesAt) {
            return false;
        }

        m_level = m_ritual->targetLevel;
        m_ritual.reset();
        return true;
    }

    void Agent::kill() {
        if (!m_alive) return;
        m_alive = false;
    }

    void Agent::setCell(Cell *cell) {
        m_cell = cell;
    }

    bool Agent::take(const ResourceType type) {
        if (!m_cell || isRitualInProgress()) {
            return false;
        }

        auto resource = m_cell->take(*this, type);

        if (!resource) {
            return false;
        }

        m_inventory.push_back(
            std::move(resource)
        );
        return true;
    }

    std::vector<InventoryResource> Agent::getInventory() const {

        std::unordered_map<ResourceType, std::size_t> count;
        for (const auto& resource: m_inventory) {
            ++count[resource->getType()];
        }

        std::vector<InventoryResource> inventory;
        for (const auto& [type, quantity]: count) {
            inventory.push_back({
                .type = type,
                .quantity =  quantity
            });
        }

        return inventory;
    }

    void Agent::updateInventory() const {
        for (const auto& resource: m_inventory) {
            resource->startRotting();
        }
    }

    std::optional<Position> Agent::move(
        Matrix &matrix,
        const Direction direction
    ) {
        if (isRitualInProgress()) {
            return std::nullopt;
        }
        return matrix.moveAgent(*this, direction);
    }

    std::optional<LookResult> Agent::look(
            const Matrix& matrix,
            const Direction direction
        ) const {

        if (!m_cell) {
            return std::nullopt;
        }

        const auto positions = matrix.getVisiblePositions(
            m_cell->getPosition(),
            direction,
            m_level
        );

        LookResult result;

        for (const auto& position: positions) {

            const auto* cell = matrix.getCell(position);

            if (!cell) {
                continue;
            }

            ObservedCell observed{
                .position = position,
                .resources = {},
                .agents = {}
            };

            for (const auto& resource: cell->getResources()) {
                observed.resources.push_back({
                    .type = resource.type,
                    .rotting = resource.rotting
                });
            }

            for (const auto& agent: cell->getAgents()) {
                if (!agent || agent == this) {
                    continue;
                }
                observed.agents.push_back({
                    .id = agent->getId(),
                    .level = agent->getLevel()
                });
            }

            result.cells.push_back(observed);
        }

        return result;
    }

}
