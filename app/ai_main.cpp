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
        exitCode = EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        exitCode = EXIT_FAILURE;
    }

    return exitCode;
}
