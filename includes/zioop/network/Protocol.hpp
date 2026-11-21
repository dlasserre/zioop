#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace network {

    inline constexpr std::uint32_t MAX_LENGTH = 4096;
    inline constexpr std::size_t HeaderSize = sizeof(std::uint16_t) + sizeof(std::uint32_t);

    class Protocol {
    public:

        struct Frame {
            std::uint16_t type;
            std::string payload;
        };

        enum class MessageType : std::uint16_t {
            // CLIENT
            HELLO = 0,
            MOVE = 1,
            LOOK = 2,
            TAKE = 3,
            DROP = 4,
            ATTACK = 5,
            BROADCAST = 6,
            INVENTORY = 7,
            RITUAL = 8,
            DISCONNECT = 9,
            // GUI
            GET_MATRIX = 10,
            GET_CLIENTS = 11,
            INVOCATION = 12,
            GIVE = 13,
            HEAL = 14
        };

        enum class ResponseType : std::uint16_t {
            // CLIENT
            OK,
            KO,
            STATE,
            // GUI
            MATRIX_STATE,
            CLIENTS_STATE,
            EVENT
        };

        struct DecodeResult {
            enum class Status {COMPLETE, INCOMPLETE, INVALID};
            Status status{Status::INCOMPLETE};
            std::optional<Frame> frame;
        };

        struct MessageHeader {
            MessageType type;
            std::uint32_t length;
        };

        struct Message {
            MessageHeader header;
            std::string payload;

            [[nodiscard]] nlohmann::json getData() const {
                return nlohmann::json::parse(payload);
            }
        };

        struct ResponseHeader {
            ResponseType type;
            std::uint32_t length;
        };

        struct Response {
            ResponseHeader header;
            std::string payload;
        };



        static std::vector<std::byte> encode(
            MessageType type,
            const std::string &payload
        );

        static std::vector<std::byte> encode(
            ResponseType type,
            const std::string &payload
        );

        static DecodeResult decode(
            const std::vector<std::byte> &data
        );

    private:
        static std::vector<std::byte> encodeRaw(
            std::uint16_t type,
            const std::string &payload
        );
    };
}
