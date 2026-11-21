#pragma once

#include <array>
#include <format>
#include <string_view>
#include <algorithm>

namespace core {
    enum class ErrorCode {
        SUCCESS = 0,
        FAILED = 1,
        INVALID_PAYLOAD,
        CLIENT_ID_ALREADY_REGISTERED,
        CELL_NOT_FOUND,
        FAILED_TO_MOVE,
        RITUAL_ALREADY_IN_PROGRESS,
        NOT_ENOUGH_RESOURCES,
        NOT_YET_READY,
        NO_REQUIRED_RESOURCES,
        FAILED_TO_CREATE_MONSTER,
        NO_WATER_ITEM,
        TARGET_NOT_FOUND,
        TARGET_TOO_FAR,
        CANT_TAKE_RESOURCE,
        CANT_GIVE_YOURSELF,
        AGENT_NOT_FOUND,
        GENERIC,
        MESSAGE_TOO_LONG
    };

    struct ErrorEntry {
        ErrorCode code;
        std::string_view reason;
    };

    constexpr std::array<ErrorEntry, 20> ERRORS {{
        {.code = ErrorCode::SUCCESS, .reason = "Success"},
        {.code = ErrorCode::INVALID_PAYLOAD, .reason = "Invalid payload"},
        {.code = ErrorCode::CLIENT_ID_ALREADY_REGISTERED, .reason = "ClientId already registered."},
        {.code = ErrorCode::CELL_NOT_FOUND, .reason = "Cell not found"},
        {.code = ErrorCode::FAILED_TO_MOVE, .reason = "Failed to move agent."},
        {.code = ErrorCode::RITUAL_ALREADY_IN_PROGRESS, .reason = "Ritual already in progress."},
        {.code = ErrorCode::NOT_ENOUGH_RESOURCES, .reason = "Not enough resources."},
        {.code = ErrorCode::NOT_YET_READY, .reason = "Agent not yet ready."},
        {.code = ErrorCode::NO_REQUIRED_RESOURCES, .reason = "You dont have required resources."},
        {.code = ErrorCode::FAILED_TO_CREATE_MONSTER, .reason = "Failed to create new monster."},
        {.code = ErrorCode::NO_WATER_ITEM, .reason = "You dont have magic_water item."},
        {.code = ErrorCode::TARGET_NOT_FOUND, .reason = "Target {} not found."},
        {.code = ErrorCode::GENERIC, .reason = "{}."},
        {.code = ErrorCode::TARGET_TOO_FAR, .reason = "Target {} is too far."},
        {.code = ErrorCode::CANT_TAKE_RESOURCE, .reason = "Cant take resource {}."},
        {.code = ErrorCode::CANT_GIVE_YOURSELF, .reason = "Cant give yourself."},
        {.code = ErrorCode::AGENT_NOT_FOUND, .reason = "Agent [{}] not found."},
        {.code = ErrorCode::MESSAGE_TOO_LONG, .reason = "Message too long."},
    }};

    constexpr std::string_view getErrorTemplate(const ErrorCode code) noexcept {
        for (const auto& entry: ERRORS) {
            if (entry.code == code) {
                return entry.reason;
            }
        }

        return "Unknown error";
    }

    struct Error {
        template <typename ...Args>
        explicit Error(const ErrorCode c, Args&& ...args) :
            m_code(c)
        {
            const std::string_view fmt = getErrorTemplate(m_code);
            if constexpr (sizeof...(args) == 0) {
                store(fmt);
            } else {
                store(std::vformat(fmt, std::make_format_args(args...)));
            }
        }

        [[nodiscard]] std::string_view message() const noexcept {
            return {m_buffer.data(), m_length};
        }

        [[nodiscard]] constexpr ErrorCode code() const noexcept {
            return m_code;
        }

    private:
        ErrorCode m_code;
        std::array<char, 256> m_buffer{};
        std::size_t m_length = 0;

        void store(const std::string_view text) noexcept {
            m_length = std::min(text.size(), m_buffer.size() - 1);

            std::copy_n(
                text.begin(),
                m_length,
                m_buffer.begin()
            );

            m_buffer[m_length] = '\0';
        }
    };
}












