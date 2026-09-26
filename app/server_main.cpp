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

int main(int argc, char *argv[]) {
    registerSignalHandlers();

    if (argc < 3) {
        std::cerr << "Usage " << argv[0] << "<width> <height>\n";
        return EXIT_FAILURE;
    }
    try {
        int width = std::stoi(argv[1]);
        int height = std::stoi(argv[2]);

        network::Server server(
            "127.0.0.1",
            9090,
            core::Matrix(width, height)
        );

        std::thread serverThread([&server] {
            std::cout << "Running server\n";
            server.Start();
        });

        while (!stopRequested) {
            const auto nextTick =
                    std::chrono::steady_clock::now() + std::chrono::milliseconds(100);

            server.processDisconnections();
            server.tick();

            std::this_thread::sleep_until(nextTick);
        }
        std::cout << "Server stopped\n";
        server.stop();

        serverThread.join();

        return EXIT_SUCCESS;
    } catch (const std::invalid_argument &) {
        std::cerr << "Invalid arguments, must be integer\n";
        return EXIT_FAILURE;
    }
}
