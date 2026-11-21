#include <stdexcept>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <print>

#include "zioop/network/Server.hpp"
#include "zioop/network/Connection.hpp"
#include "zioop/network/Protocol.hpp"
#include "zioop/network/Serialization.hpp"
#include "zioop/core/Error.hpp"

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

        void throwError(
            const Connection &connection,
            const core::AgentId &agentId,
            const core::Error &error
        ) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO,
                serializeError({
                    .base = {.agentId = agentId},
                    .errorCode = static_cast<int>(error.code()),
                    .message = std::string(error.message())
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
        m_dispatcher([this](core::ClientId clientId, const std::vector<std::byte> &frame) {
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
        const Request &request
    ) {
        const core::ClientId &clientId = request.clientId;
        const auto placement = m_game.initClient(clientId);

        if (!placement) {
            throwError(connection, core::AgentId{0}, placement.error());
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
        const Request &request
    ) {
        try {
            const core::AgentId &agentId = request.agentId;
            const core::ClientId &clientId = request.clientId;
            const auto direction = static_cast<core::Direction>(
                request.body.at("direction").get<std::uint16_t>()
            );

            const auto result = m_game.move(clientId, agentId, direction);
            if (!result) {
                throwError(connection, agentId, result.error());
                return;
            }

            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                serializePosition({
                    .base = {.agentId = agentId},
                    .position = *result->position
                }).dump()
            ));
        } catch (const nlohmann::json::exception &) {
            throwError(connection, core::AgentId{0}, core::Error{core::ErrorCode::INVALID_PAYLOAD});
        }
    }

    void Server::lookingAt(
        Connection &connection,
        const Request &request
    ) {
        try {
            const core::AgentId &agentId = request.agentId;
            const core::ClientId &clientId = request.clientId;

            const auto direction = static_cast<core::Direction>(
                request.body.at("direction").get<std::uint16_t>()
            );

            auto observation = m_game.look(clientId, agentId, direction);
            if (!observation) {
                throwError(
                    connection,
                    agentId,
                    observation.error()
                );
                return;
            }

            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                serializeLook(*observation).dump()
            ));
        } catch (const nlohmann::json::exception &) {
            throwError(connection, core::AgentId{0}, core::Error{core::ErrorCode::GENERIC, "An error occurred"});
        }
    }

    void Server::take(
        Connection &connection,
        const Request &request
    ) {
        try {
            const core::AgentId &agentId = request.agentId;
            const core::ClientId &clientId = request.clientId;
            const auto type = parseResourceType(request.body.at("type").get<int>());

            if (!type) {
                throwError(connection, agentId, core::Error{core::ErrorCode::GENERIC, "Type not found"});
                return;
            }

            const auto result = m_game.take(
                clientId,
                agentId,
                *type
            );

            if (!result) {
                throwError(connection, agentId, result.error());
                return;
            }

            connection.send(
                Protocol::encode(
                    Protocol::ResponseType::OK,
                    serializeInventory(core::InventoryResult{
                        .base = {.agentId = agentId},
                        .inventory = result->inventory,
                    }).dump()
                )
            );
        } catch (const nlohmann::json::exception &) {
            throwError(connection, core::AgentId{0}, core::Error{core::ErrorCode::GENERIC, "An error occurred"});
        }
    }

    void Server::give(
        Connection &connection,
        const Request &request
    ) {
        try {
            const core::AgentId &agentId = request.agentId;
            const auto targetID = request.body.at("targetID").get<core::AgentId>();
            const auto resourceType = parseResourceType(request.body.at("type").get<int>());

            if (!resourceType) {
                throwError(connection, agentId, core::Error{core::ErrorCode::GENERIC, "Type not found"});
                return;
            }

            const auto result = m_game.give(
                connection.getClientId(),
                agentId,
                *resourceType,
                targetID
            );

            if (!result) {
                throwError(connection, agentId, result.error());
                return;
            }

            connection.send(
                Protocol::encode(
                    Protocol::ResponseType::OK,
                    serializeGive({
                        .base = {.agentId = agentId},
                        .resource = *resourceType,
                        .targetAgentId = targetID,
                        .status = result->status
                    }).dump()
                )
            );
        } catch (const nlohmann::json::exception &e) {
            throwError(connection, core::AgentId{0}, core::Error{core::ErrorCode::GENERIC, e.what()});
        }
    }

    void Server::inventory(
        Connection &connection,
        const Request &request
    ) {
        try {
            const core::AgentId &agentId = request.agentId;
            const core::ClientId &clientId = request.clientId;

            const auto inventory = m_game.inventory(
                clientId,
                agentId
            );

            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                serializeInventory(inventory).dump()
            ));
        } catch (const nlohmann::json::exception &e) {
            throwError(connection, core::AgentId{0}, core::Error{core::ErrorCode::GENERIC, e.what()});
        }
    }

    void Server::heal(
        Connection &connection,
        const Request &request
    ) {
        try {
            const auto &agentId = request.agentId;
            const auto &clientId = request.clientId;

            const auto result = m_game.heal(clientId, agentId);
            if (!result) {
                throwError(connection, agentId, result.error());
                return;
            }
            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                serializeHeal(*result).dump()
            ));
        } catch (nlohmann::json::exception &e) {
            throwError(
                connection,
                core::AgentId{0},
                core::Error{core::ErrorCode::GENERIC, e.what()}
            );
        }
    }

    void Server::invocation(
        Connection &connection,
        const Request &request
    ) {
        try {
            const core::AgentId &agentId = request.agentId;
            const core::ClientId &clientId = request.clientId;

            const auto placement =
                    m_game.instanciatAgent(clientId, agentId);

            if (!placement) {
                throwError(
                    connection,
                    agentId,
                    placement.error()
                );
                return;
            }

            connection.send(Protocol::encode(
                Protocol::ResponseType::OK,
                serializeInvocation(*placement).dump()
            ));
        } catch (const nlohmann::json::exception &e) {
            throwError(
                connection,
                core::AgentId{0},
                core::Error{core::ErrorCode::GENERIC, e.what()}
            );
        }
    }

    void Server::attack(
        Connection &connection,
        const Request &request
    ) {
        try {
            const core::AgentId &agentId = request.agentId;
            const auto targetId = request.body.at("targetId").get<core::AgentId>();
            const auto result = m_game.attack(
                connection.getClientId(),
                agentId,
                targetId
            );

            if (!result) {
                throwError(connection, agentId, result.error());
                return;
            }

            connection.send(
                Protocol::encode(
                    Protocol::ResponseType::OK,
                    serializeAttack({
                        .base = {.agentId = agentId},
                        .targetId = targetId,
                        .killed = result->status == core::AttackStatus::KILLED
                    }).dump()
                )
            );
        } catch (const nlohmann::json::exception &e) {
            throwError(connection, core::AgentId{0}, core::Error{core::ErrorCode::GENERIC, e.what()});
        }
    }

    void Server::broadcast(
        Connection &connection,
        const Request &request
    ) {
        try {
            const core::AgentId &agentId = request.agentId;
            const auto _message = request.body.at("message").get<std::string>();

            if (_message.empty()) {
                throwError(connection, agentId, core::Error{core::ErrorCode::INVALID_PAYLOAD});
                return;
            }

            if (_message.size() > MAX_MESSAGE_SIZE) {
                throwError(connection, agentId,core::Error{core::ErrorCode::MESSAGE_TOO_LONG});
                return;
            }

            const auto result = m_game.broadcast(
                connection.getClientId(),
                agentId,
                _message
            );

            if (!result) {
                throwError(connection, agentId, result.error());
                return;
            }

            connection.send(
                Protocol::encode(
                    Protocol::ResponseType::OK,
                    serializeBroadcast(*result).dump()
                )
            );
        } catch (const nlohmann::json::exception &e) {
            throwError(connection, core::AgentId{0}, core::Error{core::ErrorCode::GENERIC, e.what()});
        }
    }

    void Server::ritual(
        Connection &connection,
        const Request &request
    ) {
        try {
            const core::AgentId &agentId = request.agentId;
            const core::ClientId &clientId = request.clientId;

            const auto result = m_game.ritual(clientId, agentId);
            if (!result) {
                throwError(connection, agentId, result.error());
                return;
            }

            connection.send(
            Protocol::encode(
                Protocol::ResponseType::OK,
                serializeRitual(*result).dump()
            ));

        } catch (const nlohmann::json::exception &e) {
            throwError(connection, core::AgentId{0}, core::Error{core::ErrorCode::GENERIC, e.what()});
        }
    }

    void Server::getMatrix(
        Connection &connection,
        const Request &
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
                std::println(stderr, "accept() failed: {}\n", WSAGetLastError());
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
                std::println("Client disconnected pulled into cleaning queue");
            };

            auto connection = std::make_shared<Connection>(
                clientSocket,
                ip,
                port,
                lambda,
                onDisconnect,
                core::ClientId{m_nextClientId++}
            );
            std::println(stdout, "New client connected ip={} on port={}\n", ip, port);
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

        struct Route {
            Handler handler;
            std::optional<core::GameCommand> command;
        };

        static const std::unordered_map<Type, Route> handlers{
            {Type::HELLO, {.handler = &Server::hello, .command = std::nullopt}},
            {Type::MOVE, {.handler = &Server::move, .command = core::GameCommand::MOVE}},
            {Type::GET_MATRIX, {.handler = &Server::getMatrix, .command = std::nullopt}},
            {Type::DISCONNECT, {.handler = &Server::disconnect, .command = std::nullopt}},
            {Type::LOOK, {.handler = &Server::lookingAt, .command = core::GameCommand::LOOK}},
            {Type::TAKE, {.handler = &Server::take, .command = core::GameCommand::TAKE}},
            {Type::INVENTORY, {.handler = &Server::inventory, .command = core::GameCommand::INVENTORY}},
            {Type::RITUAL, {.handler = &Server::ritual, .command = core::GameCommand::RITUAL}},
            {Type::ATTACK, {.handler = &Server::attack, .command = core::GameCommand::ATTACK}},
            {Type::INVOCATION, {.handler = &Server::invocation, .command = core::GameCommand::INVOCATION}},
            {Type::BROADCAST, {.handler = &Server::broadcast, .command = core::GameCommand::BROADCAST}},
            {Type::GIVE, {.handler = &Server::give, .command = core::GameCommand::GIVE}},
            {Type::HEAL, {.handler = &Server::heal, .command = core::GameCommand::HEAL}}
        };

        const auto it = handlers.find(message.header.type);
        if (it == handlers.end()) {
            connection.send(Protocol::encode(
                Protocol::ResponseType::KO,
                "Unknown or unsupported message type"
            ));
            return;
        }

        const auto &[method, command] = it->second;

        const auto data = message.getData();
        std::optional<core::AgentId> agentId;
        if (data.contains("agentID") && !data["agentID"].is_null()) {
            agentId = data["agentID"].get<core::AgentId>();
        }

        if (agentId) {
            const auto clientId = m_game.getAgentClientId(*agentId);
            if (clientId != connection.getClientId()) {
                throwError(
                    connection,
                    *agentId,
                    core::Error{core::ErrorCode::GENERIC, "ClientId does not match with the agent team."}
                );
                return;
            }

            if (command && clientId) {
                const auto start = m_game.reserve(
                    *clientId,
                    *agentId,
                    *command
                );

                std::this_thread::sleep_until(*start);
            }
        }

        const Request request{
            .agentId = agentId.value_or(core::AgentId{0}),
            .clientId = connection.getClientId(),
            .body = data
        };

        (this->*method)(connection, request);
    }

    void Server::disconnect(
        Connection &connection,
        const Request &
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
