#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <winsock2.h>
#include <thread>

namespace network {

    struct Client {
        SOCKET client{INVALID_SOCKET};
        std::thread thread;
        std::string ip;
        std::uint16_t port{};
        std::atomic_bool running{true};
    };

    class Server {
    public:
        Server(std::string address, std::uint16_t port);

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        Server(Server&&) = delete;
        Server& operator=(Server&&) = delete;

        void Start();

        void stop();

        ~Server();

    private:
        static void handleClient(Client& client);

        std::atomic_bool m_running{false};

        std::string m_address;
        std::uint16_t m_port;

        SOCKET m_socket{INVALID_SOCKET};

        std::vector<std::unique_ptr<Client>> m_clients;
    };
}

