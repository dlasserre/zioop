#include "zioop/ai/Bot.hpp"

#include <algorithm>
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

    void Bot::inventory(const std::size_t agentId) {
        nlohmann::json payload = {
            {"agentID", agentId},
        };

        std::cout << "[" << agentId << "] Asking inventory..." << std::endl;

        sendDecision(
            agentId,
            network::Protocol::MessageType::INVENTORY,
            std::move(payload)
        );
    }

    void Bot::take(const std::size_t agentId, int type) {
        nlohmann::json takePayload = {
            {"agentID", agentId},
            {"type", type}
        };

        std::cout
            << "[" << agentId << "] Taking resource: "
            << type
            << std::endl;

        sendDecision(
            agentId,
            network::Protocol::MessageType::TAKE,
            std::move(takePayload)
        );
    }

    void Bot::ritual(const std::size_t agentId) {
        sendDecision(
            agentId,
            network::Protocol::MessageType::RITUAL,
            {{"agentID", agentId}}
        );
    }

    void Bot::invocation(const std::size_t agentId) {
        std::cout << "[" << agentId << "] Invoking a new agent..." << std::endl;

        sendDecision(
            agentId,
            network::Protocol::MessageType::INVOCATION,
            {{"agentID", agentId}}
        );
    }

    void Bot::look(const std::size_t agentId, core::Direction direction) {
        nlohmann::json payload = {
            {"agentID", agentId},
            {"direction", static_cast<std::uint16_t>(direction)}
        };

        std::cout << "[" << agentId << "] Looking..." << std::endl;

        sendDecision(
            agentId,
            network::Protocol::MessageType::LOOK,
            std::move(payload)
        );
    }

    void Bot::move(const std::size_t agentId, core::Direction direction) {
        nlohmann::json payload = {
            {"agentID", agentId},
            {"direction", static_cast<std::uint16_t>(direction)}
        };

        std::cout << "[" << agentId << "] Moving..." << std::endl;

        sendDecision(
            agentId,
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
        const std::size_t agentId,
        const network::Protocol::MessageType command,
        nlohmann::json payload
    ) {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            auto &history = m_history[agentId];

            history.push_back({
                .id = history.empty() ? 1 : history.back().id + 1,
                .command = command,
                .parameters = payload,
                .response = std::nullopt,
                .reason = ""
            });
            m_pending.push_back(agentId);
        }

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

    std::optional<Bot::Decision> Bot::getPreviously(const std::size_t agentId) {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        const auto it = m_history.find(agentId);
        if (it == m_history.end() || it->second.empty()) {
            return std::nullopt;
        }

        return it->second.back();
    }

    void Bot::route(network::Protocol::Response response) {
        using MessageType = network::Protocol::MessageType;

        const auto payload = nlohmann::json::parse(
            response.payload, nullptr, false
        );

        std::optional<std::size_t> payloadAgent;
        if (payload.is_object()) {
            const auto field = payload.find("agentID");
            if (field != payload.end() && field->is_number_unsigned()) {
                payloadAgent = field->get<std::size_t>();
            }
        }

        std::size_t agentId = 0;
        std::optional<std::size_t> spawned;

        {
            std::lock_guard<std::mutex> lock(m_stateMutex);

            const auto lastCommand = [this](const std::size_t id) {
                const auto it = m_history.find(id);
                return it == m_history.end() || it->second.empty()
                    ? std::optional<MessageType>{}
                    : std::optional<MessageType>{it->second.back().command};
            };

            auto pending = m_pending.end();
            if (payloadAgent) {
                pending = std::find(
                    m_pending.begin(), m_pending.end(), *payloadAgent
                );
                if (
                    pending == m_pending.end() &&
                    !m_pending.empty() &&
                    lastCommand(m_pending.front()) == MessageType::INVOCATION
                ) {
                    pending = m_pending.begin();
                }
            } else if (!m_pending.empty()) {
                pending = m_pending.begin();
            }

            if (pending != m_pending.end()) {
                agentId = *pending;
                m_pending.erase(pending);
            } else if (payloadAgent) {
                agentId = *payloadAgent;
            } else {
                std::cerr << "Unattributed response dropped" << std::endl;
                return;
            }

            if (
                payloadAgent &&
                *payloadAgent != agentId &&
                response.header.type == network::Protocol::ResponseType::OK &&
                lastCommand(agentId) == MessageType::INVOCATION &&
                !m_history.contains(*payloadAgent)
            ) {
                m_history.try_emplace(*payloadAgent);
                spawned = payloadAgent;
            }
        }

        if (spawned) {
            enqueue({*spawned, response});
        }
        enqueue({agentId, std::move(response)});
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

    void Bot::enqueue(Incoming incoming) {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        m_queue.push(std::move(incoming));
        m_responseAvailable.notify_one();
    }

    void Bot::run(const std::atomic_bool &stopRequested) {
        std::thread thinkThread([this, &stopRequested] {
            while (true) {
                std::optional<Incoming> incoming;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_responseAvailable.wait(lock, [this, &stopRequested] {
                        return m_thinkStopped || stopRequested || !m_queue.empty();
                    });
                    if (m_thinkStopped || stopRequested) {
                        break;
                    }
                    incoming = std::move(m_queue.front());
                    m_queue.pop();
                }
                try {
                    std::cout
                        << "[" << incoming->agentId << "] "
                        << incoming->response.payload
                        << std::endl;
                    think(incoming->agentId, incoming->response);
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
            const auto agentId = data.at("agentID").get<std::size_t>();
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_history.try_emplace(agentId);
            }
            enqueue({agentId, std::move(response)});

            while (!stopRequested) {
                route(receiveResponse());
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
