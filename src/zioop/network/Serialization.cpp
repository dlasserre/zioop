#include <utility>

#include "zioop/network/Serialization.hpp"

namespace network {

    nlohmann::json serializeResource(const core::ObservedResource &resource) {
        return {
            {"type", static_cast<int>(resource.type)},
            {"rotting", resource.rotting}
        };
    }

    nlohmann::json serializeInventory(const core::InventoryResult &inventory) {
        nlohmann::json items = nlohmann::json::array({
            {"agentId", inventory.base.agentId},
        });

        for (const auto &resource: inventory.inventory) {
            items.push_back({
                {"type", resource.type},
                {"quantity", resource.quantity}
            });
        }
        return {
            {"resources", items}
        };
    }

    nlohmann::json serializeLook(const core::LookResult &observation) {
        nlohmann::json payload = {
            {"cells", nlohmann::json::array()},
            {"agentID", observation.base.agentId}
        };

        for (const auto &cell: observation.cells) {
            nlohmann::json cellJson = {
                {"position", {cell.position.x, cell.position.y}},
                {"resources", nlohmann::json::array()}
            };

            for (const auto &resource: cell.resources) {
                cellJson["resources"].push_back({
                    {"type", resource.type},
                    {"rotting", resource.rotting}
                });
            }

            for (const auto &agent: cell.agents) {
                cellJson["agents"].push_back({
                    {"id", agent.id},
                    {"level", agent.level}
                });
            }

            payload["cells"].push_back(std::move(cellJson));
        }

        return payload;
    }

    nlohmann::json serializePosition(const core::MoveResult &move) {
        return {
            {"agentID", move.base.agentId},
            {
                "position", {
                    {"x", move.position.x},
                    {"y", move.position.y}
                }
            }
        };
    }

    nlohmann::json serializeHello(const core::HelloResult &hello) {
        return {
            {"agentID", hello.base.agentId},
            {
                "position", {
                    {"x", hello.position.x},
                    {"y", hello.position.y}
                }
            }
        };
    }

    nlohmann::json serializeMatrix(const core::MatrixResult &snapshot) {
        nlohmann::json resources = nlohmann::json::array();
        nlohmann::json agents = nlohmann::json::array();

        for (const auto &resource: snapshot.snapshot.resources) {
            auto resourceJson = serializePosition({
                .position = resource.position,
            });

            resourceJson["type"] = resource.type;
            resourceJson["rotting"] = resource.rotting;
            resourceJson["subCell"] = resource.subCell;

            resources.push_back(std::move(resourceJson));
        }

        for (const auto &agent: snapshot.snapshot.agents) {
            auto agentJson = serializePosition({
                .position = agent.position,
            });

            agentJson["agentId"] = agent.agentId;
            agentJson["subCell"] = agent.subCell;
            agentJson["health"] = agent.health;
            agentJson["level"] = agent.level;

            auto &inventoryJson = agentJson["inventory"] = nlohmann::json::array();
            for (const auto &item: agent.inventory) {
                inventoryJson.push_back({
                    {"type", item.type},
                    {"quantity", item.quantity}
                });
            }

            agents.push_back(std::move(agentJson));
        }

        return {
            {
                "matrix", {
                    {"xSize", snapshot.snapshot.sizeX},
                    {"ySize", snapshot.snapshot.sizeY},
                    {
                        "data", {
                            {"resources", std::move(resources)},
                            {"agents", std::move(agents)},
                        }
                    }
                }
            }
        };
    }

    nlohmann::json serializeRitual(const core::RitualResult &ritual) {
        return {
            {"agentID", ritual.base.agentId},
            {"status", static_cast<int>(ritual.result)}
        };
    }
}
