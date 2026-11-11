#include <utility>

#include "zioop/network/Serialization.hpp"

namespace network {
    nlohmann::json serializeResource(const core::ObservedResource &resource) {
        return {
            {"type", static_cast<int>(resource.type)},
        };
    }

    nlohmann::json serializeError(const core::ErrorResult &error) {
        return {
            {"agentId", error.base.agentId},
            {"message", error.message}
        };
    }

    nlohmann::json serializeInventory(const core::InventoryResult &inventory) {
        nlohmann::json items = nlohmann::json::array({
            {"agentId", inventory.base.agentId},
        });

        for (const auto &[type, quantity]: inventory.inventory) {
            items.push_back({
                {"type", type},
                {"quantity", quantity}
            });
        }
        return {
            {"resources", items}
        };
    }

    nlohmann::json serializeGive(const core::GiveResponse &give) {
        return {
            {"agentId", give.base.agentId},
            {"resource", give.resource},
            {"targetAgentId", give.targetAgentId}
        };
    }

    nlohmann::json serializeInvocation(const core::InvocationResult &invocation) {
        return {
            {"agentId", invocation.base.agentId},
            {"spawnedAgentId", invocation.spawnedAgentId},
            {"position", {invocation.position->x, invocation.position->y}}
        };
    }

    nlohmann::json serializeLook(const core::LookResult &observation) {
        nlohmann::json payload = {
            {"cells", nlohmann::json::array()},
            {"agentId", observation.base.agentId}
        };

        for (const auto &[position, resources, agents ]: observation.cells) {
            nlohmann::json cellJson = {
                {"position", {position.x, position.y}},
                {"resources", nlohmann::json::array()}
            };

            for (const auto &[type]: resources) {
                cellJson["resources"].push_back({
                    {"type", type}
                });
            }

            for (const auto &[id, level, health]: agents) {
                cellJson["agents"].push_back({
                    {"id", id},
                    {"level", level},
                    {"health", health}
                });
            }

            payload["cells"].push_back(std::move(cellJson));
        }

        return payload;
    }

    nlohmann::json serializePosition(const core::MoveResult &move) {
        return {
            {"agentId", move.base.agentId},
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
            {"agentId", broadcast.base.agentId}
        };
    }

    nlohmann::json serializeHello(const core::HelloResult &hello) {
        return {
            {"agentId", hello.base.agentId},
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
            resourceJson["subCell"] = resource.subCell;

            resources.push_back(std::move(resourceJson));
        }

        for (const auto &agent: snapshot.snapshot.agents) {
            auto agentJson = serializePosition({
                .position = agent.position,
            });

            agentJson["agentId"] = agent.agentId;
            agentJson["teamId"] = agent.teamId;
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

            if (agent.attackTarget) {
                agentJson["attack"] = {
                    {"targetId", *agent.attackTarget},
                    {"count", agent.attackCount}
                };
            }

            if (agent.message) {
                agentJson["message"] = *agent.message;
            }

            auto &inventoryJson = agentJson["inventory"] = nlohmann::json::array();
            for (const auto &[type, quantity]: agent.inventory) {
                inventoryJson.push_back({
                    {"type", type},
                    {"quantity", quantity}
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
            {"agentId", attack.base.agentId},
            {"targetId", attack.targetId},
            {"killed", attack.killed}
        };
    }

    nlohmann::json serializeRitual(const core::RitualResult &ritual) {
        return {
            {"agentId", ritual.base.agentId},
            {"status", static_cast<int>(ritual.result)}
        };
    }

    nlohmann::json serializeEvent(const core::Message &event) {
        return {
            {"type", "Message"}, // @todo make enum
            {"speakerId", event.speakerId},
            {"agentId", event.base.agentId},
            {"message", event.message},
            {"direction", event.direction}
        };
    }

    nlohmann::json serializeEvent(const core::Died &event) {
        return {
            {"type", "Died"},
            {"agentId", event.base.agentId}
        };
    }

    nlohmann::json serializeEvent(const core::RitualCompleted &event) {
        return {
            {"type", "Ritual"},
            {"agentId", event.base.agentId},
            {"level", event.level}
        };
    }

    nlohmann::json serializeEvent(const core::DeletedRottenResources &event) {
        nlohmann::json result{
            {"type", "RottenResources"},
            {"agentId", event.base.agentId},
            {"resources", nlohmann::json::array()}
        };
        for (const auto &r: event.resources) {
            result["resources"].push_back(r);
        }
        return result;
    }
}
