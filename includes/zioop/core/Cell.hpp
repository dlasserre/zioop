#pragma once

#include "zioop/core/Resource.hpp"

#include <vector>
#include <array>
#include <memory>

namespace core {
    constexpr unsigned int MAX_AGENTS_PER_CELL = 4;

    struct Position {
        std::size_t x;
        std::size_t y;
    };

    class Agent;

    class Cell {
    public:
        [[nodiscard]]
        bool isFull() const;

        [[nodiscard]]
        bool empty() const;

        bool emplace(Agent &agent);

        bool remove(const Agent &agent);

        [[nodiscard]]
        Position getPosition() const {
            return {m_pos.x, m_pos.y};
        }

        void setPosition(const Position position) {
            this->m_pos = position;
        }

        [[nodiscard]]
        bool contains(const Agent &agent);

        [[nodiscard]]
        const std::vector<std::unique_ptr<Resource>>& getResources() const {
            return m_resources;
        }

        std::unique_ptr<Resource> take(ResourceType type);

        void addResource(ResourceType type);

        void updateResources() const;

    private:
        std::array<Agent *, MAX_AGENTS_PER_CELL> agents{};
        Position m_pos{};
        std::vector<std::unique_ptr<Resource>> m_resources{};
    };
}
