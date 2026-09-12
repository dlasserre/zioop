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

    void receiveExactly(SOCKET socket, char *buffer, int size) {
        int totalReceived = 0;

        while (totalReceived < size) {
            const int received = recv(
                socket,
                buffer + totalReceived,
                size - totalReceived,
                0
            );

            if (received == SOCKET_ERROR) {
                throw std::runtime_error(
                    "recv() failed: " + std::to_string(WSAGetLastError())
                );
            }

            if (received == 0) {
                throw std::runtime_error(
                    "Server disconnected before the complete response"
                );
            }

            totalReceived += received;
        }
    }

    void sendCommand(
        SOCKET socket,
        const network::Protocol::MessageType messageType,
        const std::string &payload
    ) {
        std::size_t totalSent = 0;
        const auto message = network::Protocol::encode(
            messageType,
            payload
        );

        while (totalSent < message.size()) {
            const int sent = send(
                socket,
                reinterpret_cast<const char *>(message.data() + totalSent),
                static_cast<int>(message.size() - totalSent),
                0
            );

            if (sent == SOCKET_ERROR) {
                throw std::runtime_error(
                    "send() failed: " + std::to_string(WSAGetLastError())
                );
            }

            if (sent == 0) {
                throw std::runtime_error("send() made no progress");
            }

            totalSent += static_cast<std::size_t>(sent);
        }
    }

    network::Protocol::Response receiveResponse(SOCKET socket) {
        using Protocol = network::Protocol;

        std::uint16_t networkType{};
        std::uint32_t networkLength{};

        receiveExactly(
            socket,
            reinterpret_cast<char *>(&networkType),
            static_cast<int>(sizeof(networkType))
        );
        receiveExactly(
            socket,
            reinterpret_cast<char *>(&networkLength),
            static_cast<int>(sizeof(networkLength))
        );

        const auto type = static_cast<Protocol::ResponseType>(ntohs(networkType));
        const std::uint32_t length = ntohl(networkLength);
        constexpr std::uint32_t MaxPayloadSize = 1024 * 1024;

        if (length > MaxPayloadSize) {
            throw std::runtime_error("Response payload too large");
        }
        if (type != Protocol::ResponseType::OK &&
            type != Protocol::ResponseType::KO) {
            throw std::runtime_error("Unexpected response type");
        }

        std::string payload(length, '\0');
        receiveExactly(socket, payload.data(), static_cast<int>(payload.size()));

        return {{type, length}, std::move(payload)};
    }
}


int main() {
    SetConsoleCtrlHandler(handleConsoleSignal, TRUE);

    WSADATA wsaData{};

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return EXIT_FAILURE;
    }

    const SOCKET socketClient = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (socketClient == INVALID_SOCKET) {
        WSACleanup();
        return EXIT_FAILURE;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(9090);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &serverAddress.sin_addr
    );

    if (
        connect(
            socketClient,
            reinterpret_cast<sockaddr *>(&serverAddress),
            sizeof(serverAddress)
        ) == SOCKET_ERROR
    ) {
        closesocket(socketClient);
        WSACleanup();
        return EXIT_FAILURE;
    }

    int exitCode = EXIT_FAILURE;

    try {
        using Protocol = network::Protocol;

        sendCommand(
            socketClient,
            Protocol::MessageType::HELLO,
            "Hello ZIOOP"
        );
        const auto helloResponse = receiveResponse(socketClient);

        std::cout
                << "HELLO "
                << (helloResponse.header.type == Protocol::ResponseType::OK ? "OK" : "KO")
                << ": " << helloResponse.payload << '\n';

        if (helloResponse.header.type == Protocol::ResponseType::OK) {
            const auto helloData = nlohmann::json::parse(helloResponse.payload);
            const auto agentId = helloData.at("agentID").get<std::size_t>();
            constexpr std::array directions{
                core::Direction::NORTH,
                core::Direction::SOUTH,
                core::Direction::EAST,
                core::Direction::WEST
            };
            std::mt19937 generator(std::random_device{}());
            std::uniform_int_distribution<std::size_t> chooseDirection(0, directions.size() - 1);

            std::cout << "Random MOVE every 3 seconds. Ctrl+C to stop." << std::endl;

            while (!stopRequested) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                if (stopRequested) {
                    break;
                }

                const auto direction = directions[chooseDirection(generator)];
                const nlohmann::json movePayload = {
                    {"agentID", agentId},
                    {"direction", static_cast<std::uint16_t>(direction)}
                };
                sendCommand(
                    socketClient,
                    Protocol::MessageType::MOVE,
                    movePayload.dump()
                );

                const auto moveResponse = receiveResponse(socketClient);
                std::cout
                        << "MOVE agent " << agentId
                        << " direction " << static_cast<std::uint16_t>(direction) << ' '
                        << (moveResponse.header.type == Protocol::ResponseType::OK ? "OK" : "KO")
                        << ": " << moveResponse.payload << std::endl;
            }

            exitCode = EXIT_SUCCESS;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }

    shutdown(socketClient, SD_SEND);
    closesocket(socketClient);
    WSACleanup();

    return exitCode;
}
