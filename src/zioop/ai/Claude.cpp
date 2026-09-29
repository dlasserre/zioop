#include "zioop/ai/Claude.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <random>
#include <optional>
#include <cstdint>

namespace ai {

void Claude::think(
    const network::Protocol::Response& response
) {
    using Protocol = network::Protocol;
    using MessageType = Protocol::MessageType;
    using ResponseType = Protocol::ResponseType;

    static thread_local std::mt19937 generator{
        std::random_device{}()
    };

    const auto previously = getPreviously();

    const auto getRandomDirection = [&] {
        std::uniform_int_distribution<std::size_t> distribution(
            0,
            directions.size() - 1
        );

        return directions[distribution(generator)];
    };

    if (!getPreviously()) {
        move(getRandomDirection());
        return;
    }

    if (response.header.type != ResponseType::OK) {
        std::cerr
            << "KO: "
            << response.payload
            << " | Previous command: "
            << static_cast<int>(previously->command)
            << std::endl;

        if (previously->command == MessageType::MOVE) {
            move(getRandomDirection());
            return;
        }

        move(getRandomDirection());
        return;
    }

    if (previously->command == MessageType::MOVE) {
        look(getRandomDirection());
        return;
    }

    if (previously->command == MessageType::LOOK) {

        nlohmann::json observation;

        try {
            observation =
                nlohmann::json::parse(response.payload);

        } catch (const nlohmann::json::exception& error) {
            std::cerr
                << "Invalid LOOK response: "
                << error.what()
                << std::endl;

            move(getRandomDirection());
            return;
        }

        if (
            !observation.contains("cells") ||
            !observation["cells"].is_array()
        ) {
            std::cerr
                << "LOOK response has no valid cells array"
                << std::endl;

            move(getRandomDirection());
            return;
        }

        for (const auto& cell : observation["cells"]) {

            if (
                !cell.contains("resources") ||
                !cell["resources"].is_array()
            ) {
                continue;
            }

            const auto& resources = cell["resources"];

            if (resources.empty()) {
                continue;
            }

            const auto& resource = resources.front();

            if (
                !resource.contains("type") ||
                !resource["type"].is_number_integer()
            ) {
                continue;
            }

            const auto type =
                resource["type"].get<int>();

            take(type);

            return;
        }

        std::cout
            << "No resources found."
            << std::endl;

        move(getRandomDirection());
        return;
    }

    if (previously->command == MessageType::TAKE) {

        std::cout
            << "Resource collected."
            << std::endl;
        inventory();
        return;
    }

    if (previously->command == MessageType::INVENTORY) {
        // Le serveur vérifie les coûts du niveau actuel et répond à la fin du rituel.
        std::cout << "Trying to level up..." << std::endl;
        ritual();
        return;
    }

    if (previously->command == MessageType::RITUAL) {
        std::cout << "Level up completed: " << response.payload << std::endl;
    }

    move(getRandomDirection());
}

}
