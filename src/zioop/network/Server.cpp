#include "zioop/network/Server.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <nlohmann/json.hpp>

#include "zioop/network/Client.hpp"
#include "zioop/network/Connection.hpp"
#include "zioop/network/Protocol.hpp"


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
    }

    Server::Server(
        std::string address,
        const std::uint16_t port,
        core::Matrix matrix
    )
        : m_matrix(std::move(matrix)),
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
        std::queue<Connection*> pending;
        {
            std::lock_guard<std::mutex> lock(m_disconnectionsMutex);
            pending.swap(m_disconnections);
        }

        while (!pending.empty()) {
            Connection *connection = pending.front();
            pending.pop();
            connection->join();

            {
                std::lock_guard<std::mutex> lock(m_stateMutex);

                const auto it = m_sessions.find(connection);

                if (it != m_sessions.end()) {
                    for (const auto& [id, agent] : it->second->getAgents()) {
                        m_matrix.removeAgent(*agent);
                    }

                    m_sessions.erase(it);
                }
            }

            // puis supprimer la session et la connexion.
        }
    }

    std::optional<Server::AgentPlacement> Server::addAgent(
        ClientSession &session,
        const core::ANGLE angle
    ) {
        core::Position target = m_matrix.angles.at(angle);

        if (m_matrix.caseIsFull(target)) {
            return std::nullopt;
        }

        auto agent = std::make_unique<core::Agent>();

        core::Agent &agentRef = *agent;

        session.addAgent(std::move(agent));

        const auto position = m_matrix.addAgent(agentRef, angle);
        if (!position) {
            return std::nullopt;
        }

        return AgentPlacement{agentRef.getId(), *position};
    }

    ClientSession *Server::getClientSession(
        Connection &connection
    ) {
        const auto it = m_sessions.find(&connection);

        if (it == m_sessions.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    void Server::hello(
        Connection &connection,
        const Protocol::Message & /* message */
    ) {
        std::optional<AgentPlacement> placement;

        {
            std::lock_guard<std::mutex> lock(m_stateMutex);

            ClientSession *session = getClientSession(connection);
            if (nullptr != session) {
                placement = addAgent(
                    *session,
                    core::BOTTOM_RIGHT
                );
            }
        }

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
                data.at("direction").get<int>()
            );
            std::optional<core::Position> position;
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                if (auto* session = getClientSession(connection)) {
                    if (auto* agent = session->getAgent(agentID)) {
                        position = m_matrix.moveAgent(*agent, direction);
                    }
                }
            }

            if (!position) {
                connection.send(
                    Protocol::encode(
                        Protocol::ResponseType::KO,
                        "Unable to move agent"
                    )
                );
                return;
            }

            const nlohmann::json payload = {
                {
                    "position", {
                        {"x", position->x},
                        {"y", position->y}
                    }
                }
            };

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

    void Server::getMatrix(
        Connection &connection,
        const Protocol::Message &
    ) {

        // @todo vérifier que la request vient bien de GUI et pas d'un client ( protection ports ).

        nlohmann::json matrix;
        {
            // Copier un etat coherent avant de relacher le verrou et d'envoyer.
            std::lock_guard<std::mutex> lock(m_stateMutex);
            nlohmann::json agents = nlohmann::json::array();

            for (const auto& [id, agent] : m_matrix.getAgents()) {
                const auto* cell = agent->getCell();
                if (!cell) {
                    continue;
                }

                agents.push_back({
                    {"agentId", id},
                    {"position", {
                        {"x", cell->pos.x},
                        {"y", cell->pos.y},
                    }}
                });
            }

            matrix = {
                {"matrix", {
                    {"xSize", m_matrix.getSizeX()},
                    {"ySize", m_matrix.getSizeY()},
                    {"data", {
                        {"resources", nlohmann::json::array()},
                        {"agents", agents}
                    }}
                }}
            };
        }

        connection.send(
            Protocol::encode(
                Protocol::ResponseType::OK,
                matrix.dump()
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

            auto connection = std::make_unique<Connection>(
                clientSocket,
                ip,
                port,
                lambda,
                onDisconnect
            );

            std::cout
                    << "New client connected ip="
                    << ip
                    << " on port="
                    << port
                    << '\n';

            Connection *connectionPtr = connection.get();
            auto session = std::make_unique<ClientSession>(*connectionPtr);

            m_connections.push_back(std::move(connection));

            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_sessions.emplace(
                    connectionPtr,
                    std::move(session)
                );
            }

            connectionPtr->start();
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

    void Server::stop() {
        m_running = false;

        if (m_socket != INVALID_SOCKET) {
            shutdown(m_socket, SD_BOTH);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        for (const auto &connection: m_connections) {
            connection->requestStop();
        }

        for (const auto &connection: m_connections) {
            connection->join();
        }
    }

    Server::~Server() {
        stop();

        m_connections.clear();

        WSACleanup();
    }
}
