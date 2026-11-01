#include <utility>

#include "zioop/network/Serialization.hpp"

namespace network {
    nlohmann::json serializeResource(const core::ObservedResource &resource) {
        return {
            {"type", static_cast<int>(resource.type)},
            {"rotting", resource.rotting}
        };
    }

    nlohmann::json serializeError(const core::ErrorResult &error) {
        return {
            {"agentID", error.base.agentId},
            {"message", error.message}
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

            for (const auto &[id, level]: cell.agents) {
                cellJson["agents"].push_back({
                    {"id", id},
                    {"level", level}
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
                    {"x", move.position->x},
                    {"y", move.position->y}
                }
            }
        };
    }

    nlohmann::json serializeBroadcast(const core::BroadcastResult &broadcast) {
        return {
            {"agentID", broadcast.base.agentId}
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
            agentJson["ritual"] = agent.ritual;
            if (agent.lookDirection) {
                agentJson["look"] = {
                    {"direction", static_cast<std::uint16_t>(*agent.lookDirection)},
                    {"count", agent.lookCount}
                };
            }
            if (agent.message) {
                agentJson["message"] = *agent.message;
            }

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

    nlohmann::json serializeAttack(const core::AttackResponse &attack) {
        return {
            {"agentID", attack.base.agentId},
            {"targetId", attack.targetId},
            {"killed", attack.killed}
        };
    }

    nlohmann::json serializeRitual(const core::RitualResult &ritual) {
        return {
            {"agentID", ritual.base.agentId},
            {"status", static_cast<int>(ritual.result)}
        };
    }

    nlohmann::json serializeEvent(const core::Message &event) {
        return {
            {"type", "Message"}, // @todo make enum
            {"speakerId", event.speakerId},
            {"agentID", event.base.agentId},
            {"message", event.message},
            {"direction", event.direction}
        };
    }

    nlohmann::json serializeEvent(const core::Died &event) {
        return {
            {"type", "Died"},
            {"agentID", event.base.agentId}
        };
    }

    nlohmann::json serializeEvent(const core::RitualCompleted &event) {
        return {
            {"type", "Ritual"},
            {"agentID", event.base.agentId},
            {"level", event.level}
        };
    }
}
