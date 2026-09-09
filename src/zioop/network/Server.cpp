#include "zioop/network/Server.hpp"

#include <iostream>
#include <ostream>
#include <stdexcept>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace network {
    static std::string getClientIP(const sockaddr_in &client) {
        char ip[INET_ADDRSTRLEN]{};

        if (inet_ntop(
                AF_INET,
                &client.sin_addr,
                ip,
                sizeof(ip)
            ) == nullptr) {
            return {};
        }

        return ip;
    }

    Server::Server(std::string address, std::uint16_t port)
        : m_address(std::move(address)), m_port(port) {
        WSAData wsaData{};

        const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

        if (result != 0) {
            throw std::runtime_error("WSAStartup failed");
        }

        m_socket = socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );

        if (m_socket == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("Could not create socket");
        }
    }

    void Server::handleClient(Client &client) {
        char buffer[1024];

        while (client.running) {
            const int received = recv(
                client.client,
                buffer,
                sizeof(buffer),
                0
            );

            if (received > 0) {
                std::cout.write(buffer, received);
                std::cout << std::endl;
            } else if (received == 0) {
                std::cout << "Client disconnected: " << client.ip << '\n';
                client.running = false;
                break;
            } else {
                std::cerr << "recv() failed: " << WSAGetLastError() << '\n';
                break;
            }
        }
    }

    void Server::stop() {
        m_running = false;

        if (m_socket != INVALID_SOCKET) {
            shutdown(m_socket, SD_BOTH);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        for (const auto &client: m_clients) {
            client->running = false;

            if (client->client != INVALID_SOCKET) {
                shutdown(client->client, SD_BOTH);
                closesocket(client->client);
                client->client = INVALID_SOCKET;
            }
        }

        for (const auto& client : m_clients) {
            if (client->thread.joinable()) {
                client->thread.join();
            }
        }
    }

    void Server::Start() {
        sockaddr_in serverAddress{};

        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(m_port);
        inet_pton(
            AF_INET,
            m_address.c_str(),
            &serverAddress.sin_addr
        );

        const int result = bind(
            m_socket,
            reinterpret_cast<sockaddr *>(&serverAddress),
            sizeof(serverAddress)
        );

        if (result == SOCKET_ERROR) {
            throw std::runtime_error("Unable to bind socket");
        }

        if (listen(m_socket, SOMAXCONN) == SOCKET_ERROR) {
            throw std::runtime_error("Unable to listen");
        }

        m_running = true;
        while (m_running) {
            sockaddr_in clientAddress{};
            int clientAddressSize = sizeof(clientAddress);

            const SOCKET clientSocket = accept(
                m_socket,
                reinterpret_cast<sockaddr *>(&clientAddress),
                &clientAddressSize
            );

            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "accept() failed\n";
                continue;
            }


            auto client = std::make_unique<Client>();
            client->client = clientSocket;
            client->ip = getClientIP(clientAddress);
            client->port = ntohs(clientAddress.sin_port);

            Client *clientPtr = client.get();

            clientPtr->thread = std::thread([this, clientPtr] {
                handleClient(*clientPtr);
            });

            m_clients.push_back(std::move(client));

            std::cout
                    << "New client connected ip=" << clientPtr->ip
                    << " on port=" << clientPtr->port
                    << '\n';
        }
    }

    Server::~Server() {
        for (const auto &client: m_clients) {
            if (client->client != INVALID_SOCKET) {
                closesocket(client->client);
                client->client = INVALID_SOCKET;
            }
        }
        m_clients.clear();

        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        WSACleanup();
    }
}
