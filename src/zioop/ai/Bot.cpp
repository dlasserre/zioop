#include "zioop/ai/Bot.hpp"

#include <iostream>
#include <random>

namespace ai {

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

    void Bot::inventory() {
        nlohmann::json payload = {
            {"agentID", getAgentId()},
        };

        std::cout << "Asking inventory..." << std::endl;

        sendDecision(
            network::Protocol::MessageType::INVENTORY,
            std::move(payload)
        );
    }

    void Bot::take(int type) {
        nlohmann::json takePayload = {
            {"agentID", getAgentId()},
            {"type", type}
        };

        std::cout
            << "Taking resource: "
            << type
            << std::endl;

        sendDecision(
            network::Protocol::MessageType::TAKE,
            std::move(takePayload)
        );
    }

    void Bot::ritual() {
        sendDecision(
            network::Protocol::MessageType::RITUAL,
            {{"agentID", getAgentId()}}
        );
    }

    void Bot::look(core::Direction direction) {
        nlohmann::json payload = {
            {"agentID", getAgentId()},
            {"direction", static_cast<std::uint16_t>(direction)}
        };

        std::cout << "Looking..." << std::endl;

        sendDecision(
            network::Protocol::MessageType::LOOK,
            std::move(payload)
        );
    }

    void Bot::move(core::Direction direction) {
        nlohmann::json payload = {
            {"agentID", getAgentId()},
            {"direction", static_cast<std::uint16_t>(direction)}
        };

        std::cout << "Moving..." << std::endl;

        sendDecision(
            network::Protocol::MessageType::MOVE,
            std::move(payload)
        );
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

    void Bot::sendDecision(
        const network::Protocol::MessageType command,
        nlohmann::json payload
    ) {
        const auto previously = getPreviously();

        const auto nextId = previously
                ? previously->id + 1
                : 1;

        addHistory({
            .id = nextId,
            .command = command,
            .parameters = payload,
            .response = std::nullopt,
            .reason = ""
        });

        sendCommand(
            command,
            payload.dump()
        );
        // @todo remove later
        std::this_thread::sleep_for(std::chrono::seconds(1));
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

    std::optional<Bot::Decision> Bot::getPreviously() {
        if (m_history.empty()) {
            return std::nullopt;
        }

        return m_history.back();
    }

    void Bot::addHistory(Decision decision) {
        m_history.push_back(std::move(decision));
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

    void Bot::run(const std::atomic_bool &stopRequested) {
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
                try {
                    std::cout << response.payload.data() << std::endl;
                    think(response);
                } catch (const std::exception &e) {
                    std::cerr << "think() failed: " << e.what() << std::endl;
                }
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
