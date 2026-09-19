#include "zioop/ai/Claude.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <random>
#include <chrono>

namespace ai {


    void Claude::think(
        const network::Protocol::Response &response
    ) {
        using Protocol = network::Protocol;

        const auto parsed = nlohmann::json::parse(response.payload);
        // std::cout << parsed.dump() << std::endl;

        std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<std::size_t> chooseDirection(0, directions.size() - 1);

        const auto direction = directions[chooseDirection(generator)];

        const nlohmann::json movePayload = {
            {"agentID", getAgentId()},
            {"direction", static_cast<std::uint16_t>(direction)}
        };

        if (Protocol::ResponseType::OK != response.header.type) {
            std::cerr << "ERROR: " << response.payload << std::endl;
            throw std::runtime_error("ERROR: " + response.payload);
        }

        auto previously = lastDecision();

        if (
            !previously ||
            previously->command == Protocol::MessageType::LOOK
        ) {
            std::cout << "Looking..." << std::endl;
            sendCommand(
                Protocol::MessageType::MOVE,
                movePayload.dump()
            );

            addHistory({
                .id = (!previously) ? 1 : previously->id + 1,
                .command = Protocol::MessageType::MOVE,
                .parameters = movePayload,
                .response = response,
                .reason = ""
            });
        } else {
            std::cout << "Moving..." << std::endl;
            sendCommand(
                Protocol::MessageType::LOOK,
                movePayload.dump()
            );
            addHistory({
                .id = (!previously) ? 1 : previously->id + 1,
                .command = Protocol::MessageType::LOOK,
                .parameters = movePayload,
                .response = response,
                .reason = ""
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

}
