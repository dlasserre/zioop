#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "zioop/ai/Bot.hpp"
#include "zioop/ai/Claude.hpp"
#include "zioop/network/Protocol.hpp"
#include "zioop/core/Matrix.hpp"

namespace {
    std::atomic_bool stopRequested{false};

    BOOL WINAPI handleConsoleSignal(DWORD signal) {
        if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
            stopRequested = true;
            return TRUE;
        }
        return FALSE;
    }
}


int main() {
    SetConsoleCtrlHandler(handleConsoleSignal, TRUE);

    int exitCode = EXIT_FAILURE;

    try {

        ai::Bot* bot = new ai::Claude("127.0.0.1", 9090);

        bot->run(stopRequested);
        // using Protocol = network::Protocol;

        // sendCommand(
        //     socketClient,
        //     Protocol::MessageType::HELLO,
        //     "Hello ZIOOP"
        // );
        // const auto helloResponse = receiveResponse(socketClient);

        //std::cout
        //        << "HELLO "
        //        << (helloResponse.header.type == Protocol::ResponseType::OK ? "OK" : "KO")
        //        << ": " << helloResponse.payload << '\n';

        //if (helloResponse.header.type == Protocol::ResponseType::OK) {
        //    const auto helloData = nlohmann::json::parse(helloResponse.payload);
        //    const auto agentId = helloData.at("agentID").get<std::size_t>();
        //    constexpr std::array directions{
        //        core::Direction::NORTH,
        //        core::Direction::SOUTH,
        //        core::Direction::EAST,
        //        core::Direction::WEST
        //    };
        //    std::mt19937 generator(std::random_device{}());
        //    std::uniform_int_distribution<std::size_t> chooseDirection(0, directions.size() - 1);

        //    std::cout << "Random MOVE every 3 seconds. Ctrl+C to stop." << std::endl;

        // while (!stopRequested) {
        //     std::this_thread::sleep_for(std::chrono::seconds(3));
        //     if (stopRequested) {
        //         break;
        //     }

        //     const auto direction = directions[chooseDirection(generator)];
        //     const nlohmann::json movePayload = {
        //         {"agentID", agentId},
        //         {"direction", static_cast<std::uint16_t>(direction)}
        //     };
        //     sendCommand(
        //         socketClient,
        //         Protocol::MessageType::MOVE,
        //         movePayload.dump()
        //     );
        //
        //     const auto moveResponse = receiveResponse(socketClient);
        //     std::cout
        //             << "MOVE agent " << agentId
        //             << " direction " << static_cast<std::uint16_t>(direction) << ' '
        //             << (moveResponse.header.type == Protocol::ResponseType::OK ? "OK" : "KO")
        //             << ": " << moveResponse.payload << std::endl;
        // }

        exitCode = EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        exitCode = EXIT_FAILURE;
    }

    // shutdown(socketClient, SD_SEND);
    // closesocket(socketClient);
    // WSACleanup();

    return exitCode;
}
