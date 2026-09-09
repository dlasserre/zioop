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

    BOOL WINAPI consoleHandler(DWORD signal)
    {
        if (signal == CTRL_C_EVENT ||
            signal == CTRL_CLOSE_EVENT)
        {
            std::cout << "Received CTRL_CLOSE_EVENT" << std::endl;
            stopRequested = true;
            return TRUE;
        }

        return FALSE;
    }

    void registerSignalHandlers()
    {
        SetConsoleCtrlHandler(consoleHandler, TRUE);
    }

#else

    void signalHandler(int signal)
    {
        if (signal == SIGINT || signal == SIGTERM) {
            stopRequested = true;
        }
    }

    void registerSignalHandlers()
    {
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
    }

#endif
}

int main() {
    registerSignalHandlers();

    std::cout << "Server started\n";
    network::Server server("127.0.0.1", 9090);

    std::thread serverThread([&server] {
        std::cout << "Running server\n";
        server.Start();
    });

    while (!stopRequested) {
        std::this_thread::sleep_for(
            std::chrono::seconds(1)
        );
    }
    std::cout << "Server stopped\n";
    server.stop();

    serverThread.join();

    return EXIT_SUCCESS;
}
