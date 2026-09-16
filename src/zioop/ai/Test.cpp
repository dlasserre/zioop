#include "zioop/ai/Test.hpp"

#include <iostream>
#include <random>

namespace ai {

    core::Direction Test::chooseDirection() {

    }

    void Test::run() {

        std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<std::size_t> chooseDirection(0, directions.size() - 1);

        std::cout << "Random MOVE every 3 seconds. Ctrl+C to stop." << std::endl;

        while (!m_stopRequested) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            if (m_stopRequested) {
                break;
            }

            const auto direction = directions[chooseDirection(generator)];
            const nlohmann::json movePayload = {
                {"agentID", m_agentId},
                {"direction", static_cast<std::uint16_t>(direction)}
            };
            sendCommand(
                network::Protocol::MessageType::MOVE,
                movePayload.dump()
            );

            const auto moveResponse = receiveResponse();
            std::cout
                    << "MOVE agent " << m_agentId
                    << " direction " << static_cast<std::uint16_t>(direction) << ' '
                    << (moveResponse.header.type == network::Protocol::ResponseType::OK ? "OK" : "KO")
                    << ": " << moveResponse.payload << std::endl;
        }
    }

}
