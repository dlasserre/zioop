#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "zioop/network/Protocol.hpp"

namespace {
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
}


int main() {
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

        const auto message = Protocol::encode(
            Protocol::MessageType::HELLO,
            "Hello ZIOOP"
        );

        // Envoyer toute la trame, même si send() n'en accepte qu'une partie.
        std::size_t totalSent = 0;

        while (totalSent < message.size()) {
            const int sent = send(
                socketClient,
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

        // Ton protocole : type sur 2 octets, longueur sur 4 octets.
        std::uint16_t networkType{};
        std::uint32_t networkLength{};

        receiveExactly(
            socketClient,
            reinterpret_cast<char *>(&networkType),
            static_cast<int>(sizeof(networkType))
        );

        receiveExactly(
            socketClient,
            reinterpret_cast<char *>(&networkLength),
            static_cast<int>(sizeof(networkLength))
        );

        const auto type = static_cast<Protocol::ResponseType>(
            ntohs(networkType)
        );
        const std::uint32_t length = ntohl(networkLength);

        // Limite de réception choisie pour ce client de test.
        constexpr std::uint32_t MaxPayloadSize = 1024 * 1024;

        if (length > MaxPayloadSize) {
            throw std::runtime_error("Response payload too large");
        }

        if (type != Protocol::ResponseType::OK &&
            type != Protocol::ResponseType::KO) {
            throw std::runtime_error("Unexpected response to HELLO");
        }

        // Lire exactement le nombre d'octets annoncé.
        std::string payload(length, '\0');

        receiveExactly(
            socketClient,
            payload.data(),
            static_cast<int>(payload.size())
        );

        std::cout
                << "Response "
                << (type == Protocol::ResponseType::OK ? "OK" : "KO")
                << ": " << payload << '\n';

        exitCode = type == Protocol::ResponseType::OK
                       ? EXIT_SUCCESS
                       : EXIT_FAILURE;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }

    closesocket(socketClient);
    WSACleanup();

    return exitCode;
}
