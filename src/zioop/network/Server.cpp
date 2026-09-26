#include "zioop/network/Server.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <nlohmann/json.hpp>

#include "zioop/network/Connection.hpp"
#include "zioop/network/Protocol.hpp"
#include "zioop/network/Serialization.hpp"


namespace network {
    namespace {
        std::string getClientIP(const sockaddr_in &client) {
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

        std::optional<core::ResourceType> parseResourceType(const int value) {
            switch (value) {
                case static_cast<int>(core::ResourceType::FRUIT):
                    return core::ResourceType::FRUIT;
                default:
                    return std::nullopt;
            }
        }
    }

    Server::Server(
        std::string address,
        const std::uint16_t port,
        core::Matrix matrix
    ) : m_game(std::move(matrix)),
        m_address(std::move(address)),
        m_port(port) {
        WSADATA wsaData{};

        if (const int result =
                    WSAStartup(MAKEWORD(2, 2), &wsaData);
            result != 0) {
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

    void Server::processDisconnections() {
        std::queue<Connection *> pending;
        {
            std::lock_guard<std::mutex> lock(m_disconnectionsMutex);
            pending.swap(m_disconnections);
        }

        while (!pending.empty()) {
            Connection *connection = pending.front();
            pending.pop();
            connection->join();

            std::shared_ptr<Connection> disconnected;
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                const auto connectionIt = std::find_if(
                    m_connections.begin(),
                    m_connections.end(),
                    [connection](const auto &value) {
                        return value.get() == connection;
                    });
                if (connectionIt != m_connections.end()) {
                    disconnected = std::move(*connectionIt);
                    m_connections.erase(connectionIt);
                }
            }

            if (disconnected) {
                m_game.removeClientAgent(disconnected->getClientId());
            }

        }
    }

    void Server::hello(
        Connection &connection,
        const Protocol::Message & /* message */
    ) {
        const auto placement = m_game.addAgent(
            connection.getClientId(), core::BOTTOM_RIGHT
        );

        if (!placement) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO,
                "Unable to create agent"
            ));
            return;
        }

        const nlohmann::json payload = {
            {"agentID", placement->agentId},
            {
                "position", {
                    {"x", placement->position.x},
                    {"y", placement->position.y}
                }
            }
        };

        connection.send(Protocol::encode(
            Protocol::ResponseType::OK,
            payload.dump()
        ));
    }

    void Server::move(
        Connection &connection,
        const Protocol::Message &message
    ) {
        try {
            const auto data = message.getData();

            const auto agentID =
                    data.at("agentID").get<std::size_t>();

            const auto direction = static_cast<core::Direction>(
                data.at("direction").get<std::uint16_t>()
            );

            const auto target = m_game.move(
                connection.getClientId(), agentID, direction
            );

            if (!target) {
                connection.send(
                    Protocol::encode(
                        Protocol::ResponseType::KO,
                        "Unable to move agent"
                    )
                );
                return;
            }

            const nlohmann::json payload = serializePosition(*target);

            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                payload.dump()
            ));
        } catch (const nlohmann::json::exception &) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO,
                "Invalid payload"
            ));
        }
    }

    void Server::lookingAt(
        Connection &connection,
        const Protocol::Message &message
    ) {
        try {
            const auto data = message.getData();

            const auto agentID =
                    data.at("agentID").get<std::size_t>();

            const auto direction = static_cast<core::Direction>(
                data.at("direction").get<std::uint16_t>()
            );

            const auto observation = m_game.look(
                connection.getClientId(), agentID, direction
            );

            if (!observation) {
                connection.send(Protocol::encode(
                    Protocol::ResponseType::KO,
                    "Agent or cell not found"
                ));
                return;
            }

            const auto payload = serializeLook(*observation);

            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                payload.dump()
            ));
        } catch (const nlohmann::json::exception &) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO,
                "Invalid payload"
            ));
        }
    }

    void Server::take(
        Connection &connection,
        const Protocol::Message &message
    ) {
        try {
            const auto data = message.getData();

            const auto agentID =
                    data.at("agentID").get<std::size_t>();

            const auto type = parseResourceType(
                data.at("type").get<int>()
            );

            if (!type) {
                connection.send(
                    Protocol::encode(
                        Protocol::ResponseType::KO,
                        "Invalid resource type"
                    )
                );
                return;
            }

            const auto result = m_game.take(
                connection.getClientId(),
                agentID,
                *type
            );

            if (!result) {
                connection.send(
                    Protocol::encode(
                        Protocol::ResponseType::KO,
                        "client not found"
                    )
                );
                return;
            }

            if (!result->taken) {
                connection.send(
                    Protocol::encode(
                        Protocol::ResponseType::KO,
                        "Unable to take resource (missing resource or ritual in progress)"
                    )
                );
                return;
            }

            connection.send(
                Protocol::encode(
                    Protocol::ResponseType::OK,
                    serializeInventory(result->inventory).dump()
                )
            );
        } catch (const nlohmann::json::exception &) {
            connection.send(
                Protocol::encode(
                    Protocol::ResponseType::KO,
                    "Invalid payload"
                )
            );
        }
    }

    void Server::inventory(
        Connection &connection,
        const Protocol::Message &message
    ) {
        // @todo deport code into inline function.
        const auto data = message.getData();

        const auto agentID =
                data.at("agentID").get<std::size_t>();

        const auto inventory = m_game.inventory(
            connection.getClientId(),
            agentID
        );

        if (!inventory) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO,
                "Inventory not found"
            ));
            return;
        }

        connection.send(Protocol::encode(
            Protocol::ResponseType::OK,
            serializeInventory(*inventory).dump()
        ));
    }

    void Server::attack(
        Connection &connection,
        const Protocol::Message &message
    ) {
        try {
            const auto data = message.getData();
            const auto agentId = data.at("agentID").get<std::size_t>();
            const auto targetId = data.at("targetId").get<std::size_t>();

            const auto result = m_game.attack(
                connection.getClientId(),
                agentId,
                targetId
            );
            const auto error = [&](std::string_view reason) {
                connection.send(Protocol::encode(
                        Protocol::ResponseType::KO,
                        nlohmann::json({
                            { "reason", reason}
                        })
                    ));
            };

            using Result = core::Game::AttackResult;
            switch (*result) {
                case Result::KILLED:
                    connection.send(Protocol::encode(
                        Protocol::ResponseType::OK, nlohmann::json({
                            {"targetId", targetId},
                            {"killed", 1}
                        })
                    ));
                    return;
                case Result::RITUAL_IN_PROGRESS:
                    error("Ritual in progress");
                    return;
                case Result::SAME_OWNER:
                    error("You cant attack yourself");
                    return;
                case Result::TARGET_NOT_FOUND:
                    error("Target not found");
                    return;
                case Result::TOO_FAR:
                    error("Target is too far from you");
                    return;
                case Result::TARGET_TOO_STRONG:
                    error("Target is too strong for you");
            }
        } catch (const nlohmann::json::exception &) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO, "Invalid payload"
            ));
        }
    }

    void Server::ritual(
        Connection &connection,
        const Protocol::Message &message
    ) {
        try {
            const auto data = message.getData();
            const auto agentId = data.at("agentID").get<std::size_t>();
            const auto result = m_game.ritual(connection.getClientId(), agentId);
            if (!result) {
                connection.send(Protocol::encode(
                    Protocol::ResponseType::KO, "Agent not found or not owned by client"
                ));
                return;
            }

            using Result = core::Game::RitualStartResult;
            switch (*result) {
                case Result::STARTED:
                    return;
                case Result::ALREADY_IN_PROGRESS:
                    connection.send(Protocol::encode(
                        Protocol::ResponseType::KO, "Ritual already in progress"
                    ));
                    return;
                case Result::NOT_ENOUGH_RESOURCES:
                    connection.send(Protocol::encode(
                        Protocol::ResponseType::KO, "Not enough resources for ritual"
                    ));
            }
        } catch (const nlohmann::json::exception &) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO, "Invalid payload"
            ));
        }
    }

    void Server::getMatrix(
        Connection &connection,
        const Protocol::Message &
    ) {
        const auto payload = serializeMatrix(m_game.getMatrix());
        connection.send(
            Protocol::encode(
                Protocol::ResponseType::OK,
                payload.dump()
            )
        );
    }

    void Server::Start() {
        sockaddr_in serverAddress{};

        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(m_port);

        if (inet_pton(
                AF_INET,
                m_address.c_str(),
                &serverAddress.sin_addr
            ) != 1) {
            throw std::runtime_error("Invalid server address");
        }

        if (bind(
                m_socket,
                reinterpret_cast<sockaddr *>(&serverAddress),
                sizeof(serverAddress)
            ) == SOCKET_ERROR) {
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
                if (!m_running) {
                    break;
                }
                std::cerr
                        << "accept() failed: "
                        << WSAGetLastError()
                        << '\n';

                continue;
            }

            std::string ip = getClientIP(clientAddress);

            const std::uint16_t port =
                    ntohs(clientAddress.sin_port);

            auto lambda = [this](Connection &connection, const Protocol::Frame &frame) {
                handleFrame(connection, frame);
            };

            auto onDisconnect = [this](Connection &disconnectedConnection) {
                std::lock_guard<std::mutex> lock_guard(m_disconnectionsMutex);
                m_disconnections.push(&disconnectedConnection);
                std::cout << "Client disconnected pulled into cleaning queue\n";
            };

            auto connection = std::make_shared<Connection>(
                clientSocket,
                ip,
                port,
                lambda,
                onDisconnect,
                m_nextClientId++
            );

            std::cout
                    << "New client connected ip="
                    << ip
                    << " on port="
                    << port
                    << '\n';

            Connection *connectionPtr = connection.get();

            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                if (!m_running) {
                    break;
                }
                m_connections.push_back(connection);
                connectionPtr->start();
            }
        }
    }

    void Server::tick() {
        const auto completed = m_game.tick();
        for (const auto &ritual: completed) {
            std::shared_ptr<Connection> connection;
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                const auto it = std::find_if(
                    m_connections.begin(),
                    m_connections.end(),
                    [&ritual](const auto &candidate) {
                        return candidate->getClientId() == ritual.clientId;
                    }
                );
                if (it != m_connections.end()) {
                    connection = *it;
                }
            }

            if (connection) {
                const nlohmann::json payload = {
                    {"agentID", ritual.agentId},
                    {"level", ritual.level}
                };
                connection->send(Protocol::encode(
                    Protocol::ResponseType::OK, payload.dump()
                ));
            }
        }
    }

    void Server::handleFrame(
        Connection &connection,
        const Protocol::Frame &frame
    ) {
        const Protocol::Message message{
            .header = {
                .type = static_cast<Protocol::MessageType>(frame.type),
                .length = static_cast<std::uint32_t>(frame.payload.size())
            },
            .payload = frame.payload
        };

        handleMessage(connection, message);
    }

    void Server::handleMessage(
        Connection &connection,
        const Protocol::Message &message
    ) {
        using Type = Protocol::MessageType;

        static const std::unordered_map<Type, Handler> handlers{
            {Type::HELLO, &Server::hello},
            {Type::MOVE, &Server::move},
            {Type::GET_MATRIX, &Server::getMatrix},
            {Type::DISCONNECT, &Server::disconnect},
            {Type::LOOK, &Server::lookingAt},
            {Type::TAKE, &Server::take},
            {Type::INVENTORY, &Server::inventory},
            {Type::RITUAL, &Server::ritual},
            {Type::ATTACK, &Server::attack},
        };

        const auto it = handlers.find(message.header.type);

        if (it == handlers.end()) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO,
                "Unknown or unsupported message type"
            ));
            return;
        }

        const Handler method = it->second;

        (this->*method)(connection, message);
    }

    void Server::disconnect(
        Connection &connection,
        const Protocol::Message &
    ) {
        connection.requestStop();
    }

    void Server::stop() {
        m_running = false;

        if (m_socket != INVALID_SOCKET) {
            shutdown(m_socket, SD_BOTH);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        std::vector<std::shared_ptr<Connection>> connections;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            connections = m_connections;
        }

        for (const auto &connection: connections) {
            connection->requestStop();
        }

        for (const auto &connection: connections) {
            connection->join();
        }
    }

    Server::~Server() {
        stop();

        m_connections.clear();

        WSACleanup();
    }
}
