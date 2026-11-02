#include <iostream>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <csignal>
#endif

#include "zioop/network/Server.hpp"

namespace {
    std::atomic_bool stopRequested{false};
#ifdef _WIN32

    BOOL WINAPI consoleHandler(DWORD signal) {
        if (signal == CTRL_C_EVENT ||
            signal == CTRL_CLOSE_EVENT) {
            std::cout << "Received CTRL_CLOSE_EVENT" << std::endl;
            stopRequested = true;
            return TRUE;
        }

        return FALSE;
    }

    void registerSignalHandlers() {
        SetConsoleCtrlHandler(consoleHandler, TRUE);
    }

#else

    void signalHandler(int signal) {
        if (signal == SIGINT || signal == SIGTERM) {
            stopRequested = true;
        }
    }

    void registerSignalHandlers() {
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
    }

#endif
}

int main(const int argc, char *argv[]) {
    registerSignalHandlers();

    try {

        //@todo clean with real arguments manager.
        const std::string host  = argc > 1 ? argv[1] : "127.0.0.1";
        const std::uint16_t port= argc > 2 ? static_cast<std::uint16_t>(std::stoi(argv[2])) : 9090;
        const int width         = argc > 3 ? std::stoi(argv[3]) : 10;
        const int height        = argc > 4 ? std::stoi(argv[4]) : 10;

        std::cout << std::format("Started on {}:{} size({}, {}) \n", host, port, width, height);

        network::Server server(
            host,
            port,
            core::Matrix(width,height)
        );

        std::thread serverThread([&server] {
            std::cout << "Running server\n";
            server.Start();
        });

        while (!stopRequested) {
            const auto nextTick =
                    std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            server.tick();

            std::this_thread::sleep_until(nextTick);
        }
        std::cout << "Server stopped" << std::endl;
        server.stop();

        serverThread.join();

        return EXIT_SUCCESS;
    } catch (const std::invalid_argument &) {
        std::cerr << "Invalid arguments, must be integer\n";
        return EXIT_FAILURE;
    }
}
