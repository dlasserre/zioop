#include <utility>

#include "zioop/network/Serialization.hpp"

namespace network {


    nlohmann::json serializeResource(const core::ObservedResource &resource) {
        return {
            {"type", static_cast<int>(resource.type)},
            {"rottng", resource.rotting}
        };
    }

    nlohmann::json serializeInventory(
        const std::vector<core::InventoryResource> &inventory
    ) {
        nlohmann::json items = nlohmann::json::array();
        for (const auto &resource: inventory) {
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
            {"cells", nlohmann::json::array()}
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

    nlohmann::json serializePosition(const core::Position &position) {
        return {
            {
                "position", {
                    {"x", position.x},
                    {"y", position.y}
                }
            }
        };
    }

    nlohmann::json serializeMatrix(const core::MatrixSnapshot &snapshot) {
        nlohmann::json resources = nlohmann::json::array();
        nlohmann::json agents = nlohmann::json::array();

        for (const auto &resource: snapshot.resources) {
            auto resourceJson = serializePosition(resource.position);

            resourceJson["type"] = static_cast<int>(resource.type);
            resourceJson["rotting"] = resource.rotting;
            resourceJson["subCell"] = resource.subCell;

            resources.push_back(std::move(resourceJson));
        }

        for (const auto &agent: snapshot.agents) {
            auto agentJson = serializePosition(agent.position);

            agentJson["agentId"] = agent.agentId;
            agentJson["subCell"] = agent.subCell;
            agentJson["health"] = agent.health;
            agentJson["level"] = agent.level;

            auto& inventoryJson = agentJson["inventory"] = nlohmann::json::array();
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
                    {"xSize", snapshot.sizeX},
                    {"ySize", snapshot.sizeY},
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
}
