#include <algorithm>
#include <random>

#include "zioop/core/Cell.hpp"
#include "zioop/core/Fruit.hpp"
#include "zioop/core/Agent.hpp"

namespace core {

    namespace {

        ResourceType getRandomResource() {
            struct Item {
                ResourceType type;
                double weight;
            };

            static const std::vector<Item> resources = {
                {.type = ResourceType::APPLE, .weight = 50.0f},
                {.type = ResourceType::MAGIC_WATER, .weight = 20.0f},
                {.type = ResourceType::CAKE, .weight = 20.0f},
                {.type = ResourceType::MEAT, .weight = 10.0f},
            };

            static std::discrete_distribution<size_t> dist = [] {
                std::vector<double> weights;
                weights.reserve(resources.size());
                for (const auto&[_, weight] : resources) {
                    weights.push_back(weight);
                }
                return std::discrete_distribution<size_t>(weights.begin(), weights.end());
            }();

            static std::mt19937 gen(std::random_device{}());

            return resources[dist(gen)].type;
        }
    }

    Cell::Cell() {
        for (int i = 0; i < MAX_RESOURCES_PER_CELL; ++i) {
            addResource(getRandomResource());
            m_nextRepop = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        }
    }

    std::size_t Cell::countResources() const {
        std::size_t count = 0;
        for (const auto &resources: m_resources) {
            count += static_cast<int>(resources.size());
        }
        return count;
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
        pushResource(type);
    }

    bool Cell::emplace(Agent &agent) {
        for (Agent *&_agent: agents) {
            if (_agent == nullptr) {
                agent.setCell(this);
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

    void Cell::repop(const Clock::time_point now) {
        if (now >= m_nextRepop && countResources() < MAX_RESOURCES_PER_CELL) {
            pushResource(getRandomResource());
            m_nextRepop = now + REPOP_TIME;
        }
    }

    std::unique_ptr<Resource> Cell::take(Agent& agent, ResourceType type) {
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
               return item->type() == type;
            });
            if (it == pile.end()) {
                continue;
            }
            agents[*current] = nullptr;
            agents[target] = &agent;

            auto resource = std::move(*it);
            pile.erase(it);
            m_nextRepop = Clock::now() + REPOP_TIME;

            return resource;
        }

        return nullptr;
    }

    std::vector<CellResource> Cell::getResources() const {
        std::vector<CellResource> result;

        for (std::size_t subCell = 0; subCell < m_resources.size(); ++subCell) {
            for (const auto &resource: m_resources[subCell]) {
                result.push_back({
                    .subCell = subCell,
                    .type = resource->type()
                });
            }
        }
        return result;
    }

}
