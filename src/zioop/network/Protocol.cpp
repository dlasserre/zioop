#include "zioop/network/Protocol.hpp"

#include <optional>
#include <winsock2.h>

namespace network {
    std::vector<std::byte> Protocol::encodeRaw(
        std::uint16_t type,
        const std::string &payload
    ) {
        const std::uint16_t networkType =
                htons(static_cast<std::uint16_t>(type));

        const std::uint32_t networkLength =
                htonl(static_cast<std::uint32_t>(payload.size()));

        std::vector<std::byte> buffer(
            HeaderSize + payload.size()
        );

        std::memcpy(
            buffer.data(),
            &networkType,
            sizeof(networkType)
        );

        std::memcpy(
            buffer.data() + sizeof(networkType),
            &networkLength,
            sizeof(networkLength)
        );

        std::memcpy(
            buffer.data() + HeaderSize,
            payload.data(),
            payload.size()
        );

        return buffer;
    }

    std::vector<std::byte> Protocol::encode(
        const MessageType type,
        const std::string &payload
    ) {
        return encodeRaw(
            static_cast<std::uint16_t>(type),
            payload
        );
    }

    std::vector<std::byte> Protocol::encode(
        const ResponseType type,
        const std::string &payload
    ) {
        return encodeRaw(
            static_cast<std::uint16_t>(type),
            payload
        );
    }

    std::optional<Protocol::Frame> Protocol::decode(
        const std::vector<std::byte> &data
    ) {
        // Pas encore assez pour lire le header
        if (data.size() < HeaderSize) {
            return std::nullopt;
        }

        std::uint16_t networkType{};
        std::uint32_t networkLength{};

        std::memcpy(
            &networkType,
            data.data(),
            sizeof(networkType)
        );

        std::memcpy(
            &networkLength,
            data.data() + sizeof(networkType),
            sizeof(networkLength)
        );

        const std::uint16_t type =
            ntohs(networkType);
        const std::uint32_t length =
                ntohl(networkLength);

        const std::size_t totalLength =
                HeaderSize + length;

        if (data.size() < totalLength) {
            return std::nullopt;
        }

        const std::string payload(
            reinterpret_cast<const char *>(
                data.data() + HeaderSize
            ),
            length
        );

        return Frame{
            .type = type,
            .payload = payload
        };
    }
}
