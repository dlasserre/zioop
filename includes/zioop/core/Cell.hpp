#pragma once

#include <vector>
#include <array>
#include <memory>
#include <optional>

#include "zioop/core/Resource.hpp"
#include "zioop/core/Types.hpp"

namespace core {
    constexpr unsigned int MAX_AGENTS_PER_CELL  = 4;
    constexpr int MAX_RESOURCES_PER_CELL        = 4;
    constexpr std::chrono::seconds REPOP_TIME(10);

    class Agent;

    struct CellResource {
        std::size_t subCell;
        ResourceType type;
    };

    class Cell {
    public:

        using Clock = std::chrono::steady_clock;

        explicit Cell();

        [[nodiscard]] bool isFull() const;

        [[nodiscard]] bool empty() const;

        [[nodiscard]] std::size_t countResources() const;

        [[nodiscard]] bool contains(const Agent &agent);

        [[nodiscard]] std::vector<CellResource> getResources() const;

        [[nodiscard]] std::unique_ptr<Resource> take(Agent &agent, ResourceType type);

        [[nodiscard]] std::optional<std::size_t> getSubCell(const Agent &agent) const;

        bool emplace(Agent &agent);

        bool remove(const Agent &agent);

        void addResource(ResourceType type);

        void repop(Clock::time_point now);

        [[nodiscard]]
        Position getPosition() const {
            return {.x = m_pos.x, .y = m_pos.y};
        }

        [[nodiscard]]
        const std::array<Agent *, MAX_AGENTS_PER_CELL>& getAgents() const {
            return agents;
        }

        void setPosition(const Position position) {
            this->m_pos = position;
        }

    private:

        struct Item {
            ResourceType type;
            double weight;
        };

        std::array<Agent *, MAX_AGENTS_PER_CELL> agents{};
        Position m_pos{};
        std::array<std::vector<std::unique_ptr<Resource>>, MAX_RESOURCES_PER_CELL> m_resources{};
        std::vector<std::unique_ptr<Resource>> m_inventory{};
        Clock::time_point m_nextRepop;

        void pushResource(ResourceType type);
        [[nodiscard]] bool hasResources() const;
    };
}
