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
                case static_cast<int>(core::ResourceType::APPLE):
                    return core::ResourceType::APPLE;
                case static_cast<int>(core::ResourceType::CAKE):
                    return core::ResourceType::CAKE;
                case static_cast<int>(core::ResourceType::MEAT):
                    return core::ResourceType::MEAT;
                case static_cast<int>(core::ResourceType::MAGIC_WATER):
                    return core::ResourceType::MAGIC_WATER;
                default:
                    return std::nullopt;
            }
        }

        void error(
            const Connection &connection,
            const std::size_t &agentId,
            const std::string &message
        ) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO,
                serializeError({
                    .base = {.agentId = agentId},
                    .message = message
                }).dump()
            ));
        }
    }

    Server::Server(
        std::string address,
        const std::uint16_t port,
        core::Matrix matrix
    ) : m_game(std::move(matrix)),
        m_address(std::move(address)),
        m_port(port),
        m_dispatcher([this](std::size_t clientId, const std::vector<std::byte> &frame) {
            std::shared_ptr<Connection> connection;
            {
                std::lock_guard lock(m_stateMutex);
                const auto it = std::ranges::find_if(
                    m_connections.begin(),
                    m_connections.end(),
                    [&](const auto &client) {
                        return client->getClientId() == clientId;
                    });
                if (it != m_connections.end()) {
                    connection = *it;
                }
            }
            if (connection) connection->send(frame);
        }) {
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
            std::lock_guard lock(m_disconnectionsMutex);
            pending.swap(m_disconnections);
        }

        while (!pending.empty()) {
            Connection *connection = pending.front();
            pending.pop();
            connection->join();

            std::shared_ptr<Connection> disconnected;
            {
                std::lock_guard lock(m_stateMutex);
                const auto connectionIt = std::ranges::find_if(
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
        const auto placement = m_game.initClient(
            connection.getClientId()
        );

        if (!placement) {
            error(connection, 0, "Unable to create agent");
            return;
        }

        connection.send(Protocol::encode(
            Protocol::ResponseType::OK,
            serializeHello({
                .base = {.agentId = placement->agentId},
                .position = placement->position
            }).dump()
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

            if (!target || !target->position) {
                error(connection, agentID, "Unable to move agent");
                return;
            }

            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                serializePosition({
                    .base = {.agentId = agentID},
                    .position = *target->position
                }).dump()
            ));
        } catch (const nlohmann::json::exception &) {
            error(connection, 0, "Invalid payload");
        }
    }

    void Server::lookingAt(
        Connection &connection,
        const Protocol::Message &message
    ) {
        const auto data = message.getData();

        const auto agentID =
                data.at("agentID").get<std::size_t>();

        try {
            const auto direction = static_cast<core::Direction>(
                data.at("direction").get<std::uint16_t>()
            );

            auto observation = m_game.look(
                connection.getClientId(), agentID, direction
            );

            if (!observation) {
                error(connection, agentID, std::format("Agent {} or cell not found", agentID));
                return;
            }
            observation->base.agentId = agentID;
            const auto payload = serializeLook(*observation);

            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                payload.dump()
            ));
        } catch (const nlohmann::json::exception &) {
            error(connection, 0, "An error occurred");
        }
    }

    void Server::take(
        Connection &connection,
        const Protocol::Message &message
    ) {
        const auto data = message.getData();

        const auto agentID =
                data.at("agentID").get<std::size_t>();

        try {
            const auto type = parseResourceType(
                data.at("type").get<int>()
            );

            if (!type) {
                error(connection, agentID, "Type not found");
                return;
            }

            const auto result = m_game.take(
                connection.getClientId(),
                agentID,
                *type
            );

            if (!result) {
                error(
                    connection,
                    agentID,
                    std::format("Client({}) not found", connection.getClientId())
                );
                return;
            }

            if (!result->taken) {
                error(
                    connection,
                    agentID,
                    "Unable to take resource (missing resource or ritual  in progress)"
                );
                return;
            }

            connection.send(
                Protocol::encode(
                    Protocol::ResponseType::OK,
                    serializeInventory(core::InventoryResult{
                        .base = {.agentId = agentID},
                        .inventory = result->inventory,
                    }).dump()
                )
            );
        } catch (const nlohmann::json::exception &) {
            error(connection, 0, "An error occurred");
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
            error(connection, agentID, "Inventory not found");
            return;
        }

        connection.send(Protocol::encode(
            Protocol::ResponseType::OK,
            serializeInventory(*inventory).dump()
        ));
    }

    void Server::invocation(
        Connection &connection,
        const Protocol::Message &message
    ) {
        const auto data = message.getData();
        const auto agentID =
                data.at("agentID").get<std::size_t>();

        try {
            const auto placement =
                m_game.createAgent(connection.getClientId(), agentID);

            if (!placement) {
                error(connection, agentID, "Cannot spawn new agent, matrix is probably full");
                return;
            }

            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                serializeInvocation(*placement).dump()
            ));
        } catch (const nlohmann::json::exception &e) {
            error(connection, agentID, std::format("An error occurred: {}", e.what()));
        }
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
            if (!result) {
                error(connection, agentId, "An error occurred");
                return;
            }

            using Result = core::AttackResult;
            switch (*result) {
                case Result::DAMAGED:
                    connection.send(
                        Protocol::encode(
                            Protocol::ResponseType::OK,
                            serializeAttack({
                                .base = {.agentId = agentId},
                                .targetId = targetId,
                                .killed = false
                            }).dump()
                        )
                    );
                    return;
                case Result::KILLED:
                    connection.send(Protocol::encode(
                        Protocol::ResponseType::OK,
                        serializeAttack({
                            .base = {.agentId = agentId},
                            .targetId = targetId,
                            .killed = true
                        }).dump()
                    ));
                    return;
                case Result::RITUAL_IN_PROGRESS:
                    error(connection, agentId, "Ritual in progress");
                    return;
                case Result::SAME_OWNER:
                    error(connection, agentId, "You cant attack yourself");
                    return;
                case Result::TARGET_NOT_FOUND:
                    error(connection, agentId, "Target not found");
                    return;
                case Result::TOO_FAR:
                    error(connection, agentId, "Target is too far from you");
                    return;
                case Result::TARGET_TOO_STRONG:
                    error(connection, agentId, "Target is too strong for you");
            }
        } catch (const nlohmann::json::exception &e) {
            error(connection, 0, e.what());
        }
    }

    void Server::broadcast(
        Connection &connection,
        const Protocol::Message &message
    ) {
        try {
            const auto data = message.getData();
            const auto agentId = data.at("agentID").get<std::size_t>();
            auto _message = data.at("message").get<std::string>();

            if (_message.empty()) {
                error(connection, agentId, "Message is empty");
                return;
            }

            if (_message.size() > 255) {
                error(connection, agentId, "Message too long");
                return;
            }

            const auto result = m_game.broadcast(
                connection.getClientId(),
                agentId,
                std::move(_message)
            );

            if (!result) {
                error(connection, agentId, "Cant broadcast message");
                return;
            }

            connection.send(
                Protocol::encode(
                    Protocol::ResponseType::OK,
                    serializeBroadcast(*result).dump()
                )
            );
        } catch (const nlohmann::json::exception &e) {
            error(connection, 0, e.what());
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
                error(connection, agentId, "Agent not found or not owned by client");
                return;
            }

            switch (result->result) {
                case core::RitualStartResult::STARTED:
                    connection.send(
                        Protocol::encode(
                            Protocol::ResponseType::OK,
                            serializeRitual(*result).dump()
                        ));
                    return;
                case core::RitualStartResult::ALREADY_IN_PROGRESS:
                    error(connection, agentId, "Ritual already in progress");
                    return;
                case core::RitualStartResult::NOT_ENOUGH_RESOURCES:
                    error(connection, agentId, "Not enough resources for ritual");
            }
        } catch (const nlohmann::json::exception &e) {
            error(connection, 0, e.what());
        }
    }

    void Server::getMatrix(
        Connection &connection,
        const Protocol::Message &
    ) {
        const auto payload = serializeMatrix({
            .snapshot = m_game.getMatrix()
        });
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
                std::cerr << std::format("accept() failed: {}\n", WSAGetLastError());
                continue;
            }

            std::string ip = getClientIP(clientAddress);

            const std::uint16_t port =
                    ntohs(clientAddress.sin_port);

            auto lambda = [this](Connection &connection, const Protocol::Frame &frame) {
                handleFrame(connection, frame);
            };

            auto onDisconnect = [this](Connection &disconnectedConnection) {
                std::lock_guard lock_guard(m_disconnectionsMutex);
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

            std::cout << std::format("New client connected ip={} on port={}\n", ip, port);
            {
                Connection *connectionPtr = connection.get();
                std::lock_guard lock(m_stateMutex);
                if (!m_running) {
                    break;
                }
                m_connections.push_back(connection);
                connectionPtr->start();
            }
        }
    }

    void Server::tick() {
        processDisconnections();
        m_game.tick(std::chrono::steady_clock::now());
        m_dispatcher.dispatch(m_game.drainEvents());
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
            {Type::INVOCATION, &Server::invocation},
            {Type::BROADCAST, &Server::broadcast}
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

        std::vector<std::shared_ptr<Connection> > connections;
        {
            std::lock_guard lock(m_stateMutex);
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
