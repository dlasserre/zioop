#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "zioop/network/Server.hpp"

// A test-only entry point with a selectable port and a graceful stdin shutdown.
int main(const int argc, char *argv[]) {
    if (argc != 2) {
        return 1;
    }

    network::Server server(
        "127.0.0.1", static_cast<std::uint16_t>(std::stoi(argv[1])), core::Matrix(12, 12)
    );
    std::atomic_bool stopRequested{false};
    std::thread listener([&server] { server.Start(); });
    std::thread stopReader([&stopRequested] {
        std::cin.get();
        stopRequested = true;
    });

    while (!stopRequested) {
        server.processDisconnections();
        server.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    server.stop();
    listener.join();
    stopReader.join();
}
