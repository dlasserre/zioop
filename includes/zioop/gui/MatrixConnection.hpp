#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <raylib.h>

#include <optional>

#include "zioop/network/Client.hpp"

namespace gui {

    constexpr std::array<Vector2, 4> CELL_OFFSETS{{
        {-0.25f, -0.25f}, {0.25f, -0.25f},
        {-0.25f, 0.25f}, {0.25f, 0.25f}
    }};

    class MatrixConnection {
    public:
        struct InventoryItem {
            int type{};
            std::size_t quantity{};
        };

        struct AgentView {
            std::size_t x{};
            std::size_t y{};
            Vector3 position{};
            std::string action{};
            std::size_t actionCount{};
            std::size_t level{};
            std::size_t teamId{};
            int health{};
            int maxHealth{100};
            std::vector<InventoryItem> inventory;
            bool ritual{false};
            std::string message{};
            std::optional<int> lookDirection{};
            std::size_t lookCount{};
            std::optional<std::size_t> attackTarget{};
            std::size_t attackCount{};
        };

        struct ResourceView {
            int type{};
            Vector3 position{};
        };

        struct MatrixSnapshot {
            std::size_t width{};
            std::size_t height{};
            std::map<std::size_t, AgentView> agents;
            std::vector<ResourceView> resources;
        };

        MatrixConnection();

        MatrixConnection(const MatrixConnection &) = delete;

        MatrixConnection &operator=(const MatrixConnection &) = delete;

        ~MatrixConnection();

        MatrixSnapshot request();

    private:
        SOCKET m_socket = INVALID_SOCKET;

        void close();

        void open();

        void receiveExactly(char *buffer, int size) const;
    };
}
