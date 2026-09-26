#include "zioop/core/Cell.hpp"
#include "zioop/core/Fruit.hpp"

#include <algorithm>

namespace core {

    Cell::Cell() {
        m_repopThread = std::thread(&Cell::repopLoop, this);
    }

    Cell::Cell(Cell&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.m_mutex);
        m_pos = other.m_pos;
        m_resources = std::move(other.m_resources);
        m_inventory = std::move(other.m_inventory);
        agents = other.agents;
        m_running = other.m_running;
        m_repopThread = std::move(other.m_repopThread);
    }

    Cell& Cell::operator=(Cell&& other) noexcept {
        if (this != &other) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_running = false;
            }
            m_cv.notify_one();
            if (m_repopThread.joinable()) {
                m_repopThread.join();
            }
            std::lock_guard<std::mutex> lock(other.m_mutex);
            m_pos = other.m_pos;
            m_resources = std::move(other.m_resources);
            m_inventory = std::move(other.m_inventory);
            agents = other.agents;
            m_running = other.m_running;
            m_repopThread = std::move(other.m_repopThread);
        }
        return *this;
    }

    Cell::~Cell() {
        {
            std::lock_guard<std::mutex> lock_guard(m_mutex);
            m_running = false;
        }

        m_cv.notify_one();

        if (m_repopThread.joinable()) {
            m_repopThread.join();
        }
    }

    bool Cell::hasResources() const {
        return std::ranges::any_of(m_resources, [](const auto& pile) {
            return !pile.empty();
        });
    }

    void Cell::pushResource(const ResourceType type) {
        const auto pile = std::ranges::min_element(m_resources, {}, [](const auto& resources) {
            return resources.size();
        });
        pile->push_back(std::make_unique<Fruit>(type));
    }

    std::optional<std::size_t> Cell::getSubCell(const Agent &agent) const {
        for (std::size_t subCell = 0; subCell < agents.size(); ++subCell) {
            if (agents[subCell] == &agent) {
                return subCell;
            }
        }

        return std::nullopt;
    }

    void Cell::addResource(const ResourceType type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        pushResource(type);
    }


    void Cell::repopLoop() {
        while (true) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] {
                return !hasResources() || !m_running;
            });

            if (!m_running) {
                break;
            }

            for (size_t i = 0; i < m_maxResources; ++i) {
                pushResource(ResourceType::FRUIT);
            }
        }
    }

    bool Cell::emplace(Agent &agent) {
        for (Agent *&_agent: agents) {
            if (_agent == nullptr) {
                _agent = &agent;
                return true;
            }
        }

        return false;
    }

    bool Cell::isFull() const {
        return std::ranges::all_of(agents, [](const Agent *agent) {
            return agent != nullptr;
        });
    }

    bool Cell::empty() const {
        return std::ranges::all_of(agents, [](const Agent *agent) {
            return agent == nullptr;
        });
    }

    bool Cell::contains(const Agent &agent) {
        return std::ranges::any_of(agents, [&agent](Agent *&_agent) {
            return &agent == _agent;
        });
    }

    bool Cell::remove(const Agent &agent) {
        for (Agent *&_agent: agents) {
            if (_agent == &agent) {
                _agent = nullptr;
                return true;
            }
        }

        return false;
    }

    std::unique_ptr<Resource> Cell::take(Agent& agent, ResourceType type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto current = getSubCell(agent);
        if (!current) {
            return nullptr;
        }

        for (std::size_t step = 0; step < MAX_AGENTS_PER_CELL; ++step) {
            const auto target = *current ^ step;
            if (agents[target] != nullptr && agents[target] != &agent) {
                continue;
            }
            auto &pile = m_resources[target];
            const auto it = std::ranges::find_if(pile, [type](const auto &item) {
               return item->getType() == type;
            });
            if (it == pile.end()) {
                continue;
            }
            agents[*current] = nullptr;
            agents[target] = &agent;

            auto resource = std::move(*it);
            pile.erase(it);
            m_cv.notify_one();

            return resource;
        }

        return nullptr;
    }

    std::vector<CellResource> Cell::getResources() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<CellResource> result;
        for (std::size_t subCell = 0; subCell < m_resources.size(); ++subCell) {
            for (const auto &resource: m_resources[subCell]) {
                result.push_back({
                    .subCell = subCell,
                    .type = resource->getType(),
                    .rotting = resource->getRotting()
                });
            }
        }
        return result;
    }

    void Cell::updateResources() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto &pile: m_resources) {
            for (const auto &resource: pile) {
                resource->startRotting();
            }
        }
    }

}
