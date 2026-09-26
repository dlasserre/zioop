#pragma once

#include "zioop/core/Resource.hpp"
#include "zioop/core/Types.hpp"

#include <vector>
#include <array>
#include <condition_variable>
#include <memory>
#include <thread>
#include <iostream>
#include <mutex>
#include <optional>

namespace core {
    constexpr unsigned int MAX_AGENTS_PER_CELL = 4;

    class Agent;

    struct CellResource {
        std::size_t subCell;
        ResourceType type;
        int rotting;
    };

    class Cell {
    public:

        explicit Cell();

        Cell(const Cell&) = delete;
        Cell& operator=(const Cell&) = delete;

        Cell(Cell&& other) noexcept;
        Cell& operator=(Cell&& other) noexcept;

        ~Cell();

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

        void addResource(ResourceType type);

        std::optional<std::size_t> getSubCell(const Agent &agent) const;

        void setPosition(const Position position) {
            this->m_pos = position;
        }

        [[nodiscard]]
        bool contains(const Agent &agent);

        [[nodiscard]]
        std::vector<CellResource> getResources() const;

        [[nodiscard]]
        std::unique_ptr<Resource> take(Agent &agent, ResourceType type);

        [[nodiscard]]
        const std::array<Agent *, MAX_AGENTS_PER_CELL>& getAgents() const {
            return agents;
        }

        void updateResources() const;

    private:

        void pushResource(ResourceType type);
        bool hasResources() const;
        void repopLoop();

        std::array<Agent *, MAX_AGENTS_PER_CELL> agents{};
        Position m_pos{};
        std::array<std::vector<std::unique_ptr<Resource>>, MAX_AGENTS_PER_CELL> m_resources{};
        std::vector<std::unique_ptr<Resource>> m_inventory{};
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::thread m_repopThread;
        bool m_running{true};
        const std::size_t m_maxResources{4};
    };
}
