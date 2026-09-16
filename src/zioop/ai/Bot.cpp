#include "zioop/ai/Bot.hpp"

#include <iostream>

namespace ai {
    void Bot::connect() const {
        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(m_port);

        inet_pton(
            AF_INET,
            m_ip.c_str(),
            &serverAddress.sin_addr
        );

        if (
            ::connect(
                m_socket,
                reinterpret_cast<sockaddr *>(&serverAddress),
                sizeof(serverAddress)
            ) == SOCKET_ERROR
        ) {
            closesocket(m_socket);
            WSACleanup();
            throw std::runtime_error("Unable to connect to server");
        }
    }



    Bot::Bot(
        std::string ip,
        const std::uint16_t port
    ) : m_ip(std::move(ip)), m_port(port) {
        WSADATA wsaData{};

        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("Unable to initialize WSAStartup");
        }

        m_socket = socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );

        if (m_socket == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("Unable to initialize socket");
        }
    }

    void Bot::sendCommand(
        const network::Protocol::MessageType messageType,
        const std::string &payload
    ) const {
        std::size_t totalSent = 0;
        const auto message = network::Protocol::encode(
            messageType,
            payload
        );

        while (totalSent < message.size()) {
            const int sent = send(
                m_socket,
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

    void Bot::receiveExactly(
        char *buffer,
        const int size)
    const {
        int totalReceived = 0;

        while (totalReceived < size) {
            const int received = recv(
                m_socket,
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

    network::Protocol::Response Bot::receiveResponse() const {
        using Protocol = network::Protocol;

        std::uint16_t networkType{};
        std::uint32_t networkLength{};

        receiveExactly(
            reinterpret_cast<char *>(&networkType),
            static_cast<int>(sizeof(networkType))
        );
        receiveExactly(
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
        receiveExactly(payload.data(), static_cast<int>(payload.size()));

        return {{type, length}, std::move(payload)};
    }

    void Bot::enqueue(network::Protocol::Response response) {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        m_queue.push(std::move(response));
        m_responseAvailable.notify_one();
    }

    void Bot::run(
        const std::atomic_bool &stopRequested
    ) {
        std::thread thinkThread([this, &stopRequested] {
            while (true) {
                network::Protocol::Response response;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_responseAvailable.wait(lock, [this, &stopRequested] {
                        return m_thinkStopped || stopRequested || !m_queue.empty();
                    });
                    if (m_thinkStopped || stopRequested) {
                        break;
                    }
                    response = std::move(m_queue.front());
                    m_queue.pop();
                }
                think(response);
            }
        });

        try {
            this->connect();
            sendCommand(
                network::Protocol::MessageType::HELLO,
                "Hello from new client (" + std::string(getName()) + ").\n"
            );
            network::Protocol::Response response = receiveResponse();
            if (response.header.type != network::Protocol::ResponseType::OK) {
                throw std::runtime_error(response.payload);
            }
            const auto data = nlohmann::json::parse(response.payload);
            m_agentId = data.at("agentID").get<std::size_t>();
            enqueue(std::move(response));

            while (!stopRequested) {
                enqueue(receiveResponse());
            }

            disconnect();
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
            disconnect();
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_thinkStopped = true;
        }
        m_responseAvailable.notify_all();

        if (thinkThread.joinable()) {
            thinkThread.join();
        }
    }

    void Bot::disconnect() {
        if (m_socket == INVALID_SOCKET) {
            return;
        }

        try {
            sendCommand(
                network::Protocol::MessageType::DISCONNECT,
                "Bye"
            );
        } catch (...) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            throw;
        }

        shutdown(m_socket, SD_SEND);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    Bot::~Bot() {
        disconnect();
    }
}
