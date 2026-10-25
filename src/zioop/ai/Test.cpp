#include "zioop/ai/Test.hpp"

#include <iostream>
#include <random>

namespace ai {

    void Test::think(
        const std::size_t agentId,
        const network::Protocol::Response &response
    ) {
        using MessageType = network::Protocol::MessageType;

        static thread_local std::mt19937 generator{std::random_device{}()};
        std::uniform_int_distribution<std::size_t> distribution(
            0, directions.size() - 1
        );

        const auto previously = getPreviously(agentId);

        if (!previously && !m_invoked.exchange(true)) {
            invocation(agentId);
            return;
        }

        if (previously && previously->command == MessageType::INVOCATION) {
            std::cout
                    << "INVOCATION "
                    << (response.header.type == network::Protocol::ResponseType::OK ? "OK" : "KO")
                    << ": " << response.payload << std::endl;
        }

        move(agentId, directions[distribution(generator)]);
    }

}
