#include <utility>
#include <algorithm>
#include <unordered_map>

#include "zioop/core/Inventory.hpp"
#include "zioop/core/Matrix.hpp"
#include "zioop/core/Agent.hpp"

namespace core {
    constexpr int DAMAGE = 2;

    namespace {
        std::uint64_t nextAgentId = 1;
    }

    Agent::Agent(const ClientId clientId)
        : m_clientId(clientId), m_id(AgentId{nextAgentId++}) {
    }

    bool Agent::startRitual(
        const std::chrono::seconds duration,
        const std::chrono::steady_clock::time_point now
    ) {
        if (isRitualInProgress()) {
            return false;
        }
        const auto index = static_cast<std::size_t>(getLevel() - 1);
        if (
            index >= RITUAL_REQUIREMENTS.size() ||
            !m_inventory.remove(RITUAL_REQUIREMENTS[index])
        ) {
            return false;
        }

        m_ritual = Ritual{
            .targetLevel = m_level + 1,
            .finishesAt = now + duration
        };

        return true;
    }

    AttackStatus Agent::attack(Agent* target) {
        if (!target) {
            return AttackStatus::TARGET_NOT_FOUND;
        }

        if (isRitualInProgress() || target->isRitualInProgress()) {
            return AttackStatus::RITUAL_IN_PROGRESS;
        }

        if (getClientId() == target->getClientId()) {
            return AttackStatus::SAME_OWNER;
        }

        if (!getCell() || getCell() != target->getCell()) {
            return AttackStatus::TOO_FAR;
        }

        if (getLevel() < target->getLevel()) {
            return AttackStatus::TARGET_TOO_STRONG;
        }

        if (target->killed()) {
            return AttackStatus::TARGET_NOT_FOUND;
        }

        m_attackTarget = target->getId();
        ++m_attackCount;
        target->damage(getLevel() * DAMAGE);

        return target->killed() ? AttackStatus::KILLED : AttackStatus::DAMAGED;
    }

    bool Agent::completeRitual(const std::chrono::steady_clock::time_point now) {
        if (!m_ritual || now < m_ritual->finishesAt) {
            return false;
        }

        m_level = m_ritual->targetLevel;
        m_ritual.reset();
        return true;
    }

    void Agent::setFullLife() {
        const auto level = std::max(1, getLevel());
        const int hp = BASE_HP + (level - 1) * HP_PER_LEVEL;
        m_health = hp;
    }

    bool Agent::damage(const int damage) {
        if (killed()) {
            return false;
        }
        m_health = std::max(0, m_health - damage);
        return true;
    }

    void Agent::setCell(Cell *cell) {
        m_cell = cell;
    }

    [[nodiscard]] bool Agent::take(const ResourceType type) {
        if (!m_cell || isRitualInProgress()) {
            return false;
        }

        const auto resource = m_cell->take(*this, type);

        if (!resource) {
            return false;
        }

        m_inventory.add(*resource, Inventory::Clock::now());

        return true;
    }

    std::vector<InventoryResource> Agent::getItems() const {
        std::vector<InventoryResource> result;

        for (const auto &[type, quantity]: m_inventory.getItems()) {
            result.push_back({
                .type = type,
                .quantity = quantity
            });
        }

        return result;
    }

    Inventory &Agent::getInventory() {
        return m_inventory;
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

        LookResult result{.base = { .agentId = m_id }};

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
                    .type = resource.type
                });
            }

            for (const auto &agent: cell->getAgents()) {
                if (!agent || agent == this) {
                    continue;
                }
                observed.agents.push_back({
                    .id = agent->getId(),
                    .level = agent->getLevel(),
                    .health = agent->getHealth()
                });
            }

            result.cells.push_back(observed);
        }

        return result;
    }

    std::optional<Direction> Agent::getLookDirection() const {
        return m_lookDirection;
    }

    std::size_t Agent::getLookCount() const {
        return m_lookCount;
    }

    std::optional<AgentId> Agent::getAttackTarget() const {
        return m_attackTarget;
    }

    std::size_t Agent::getAttackCount() const {
        return m_attackCount;
    }

    ClientId Agent::getClientId() const {
        return m_clientId;
    }

    std::chrono::steady_clock::time_point Agent::reserve(
        const std::chrono::milliseconds duration,
        const Clock::time_point now
    ) {
        const auto start = std::max(now, m_readyAt);
        m_readyAt = start + duration;
        return start;
    }

    void Agent::say(std::string message, const std::chrono::steady_clock::time_point until) {
        m_speech = Speech{.message = std::move(message), .until = until};
    }

    void Agent::recordLook(const Direction direction) {
        m_lookDirection = direction;
        ++m_lookCount;
    }

    std::optional<std::string> Agent::getSpeech(const std::chrono::steady_clock::time_point now) const {
        if (!m_speech || now >= m_speech->until) {
            return std::nullopt;
        }
        return m_speech->message;
    }
}
