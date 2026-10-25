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

    bool Agent::consumeRequiredResourcesForLevel() {
        const auto it = std::find_if(
            m_requiredResources.begin(),
            m_requiredResources.end(),
            [&](const LevelResources &entry) {
                return entry.level == m_level;
            }
        );

        if (it == m_requiredResources.end()) {
            return false;
        }

        for (const auto &[type, requiredCount]: it->resources) {
            const auto available = std::count_if(
                m_inventory.begin(),
                m_inventory.end(),
                [type](const auto &item) {
                    return item->getType() == type;
                }
            );

            if (static_cast<std::size_t>(available) < requiredCount) {
                return false;
            }
        }

        for (const auto &[type, requiredCount]: it->resources) {
            std::size_t remaining = requiredCount;

            for (auto item = m_inventory.begin();
                 item != m_inventory.end() && remaining > 0;) {
                if ((*item)->getType() == type) {
                    item = m_inventory.erase(item);
                    --remaining;
                } else {
                    ++item;
                }
            }
        }

        return true;
    }

    bool Agent::startRitual(
        const std::chrono::seconds duration,
        const std::chrono::steady_clock::time_point now
    ) {
        if (isRitualInProgress()) {
            return false;
        }

        if (!consumeRequiredResourcesForLevel()) {
            return false;
        }

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

    bool Agent::damage(const int damage) {
        if (!m_alive) {
            return false;
        }
        m_health -= (m_health - damage < 0) ? damage : 0;
        return true;
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
        for (const auto &resource: m_inventory) {
            ++count[resource->getType()];
        }
        std::vector<InventoryResource> inventory;
        inventory.reserve(count.size());
        for (const auto &[type, quantity]: count) {
            inventory.push_back({
                .type = type,
                .quantity = quantity
            });
        }

        return inventory;
    }

    void Agent::updateInventory() const {
        for (const auto &resource: m_inventory) {
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
        const Matrix &matrix,
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

        for (const auto &position: positions) {
            const auto *cell = matrix.getCell(position);

            if (!cell) {
                continue;
            }

            ObservedCell observed{
                .position = position,
                .resources = {},
                .agents = {}
            };

            for (const auto &resource: cell->getResources()) {
                observed.resources.push_back({
                    .type = resource.type,
                    .rotting = resource.rotting
                });
            }

            for (const auto &agent: cell->getAgents()) {
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
