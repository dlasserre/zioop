#pragma once

#include "Bot.hpp"

namespace ai {
    class Test : public Bot {
    public:
        using Bot::Bot;

    protected:
        [[nodiscard]] std::string_view getName() const override {
            return "Test";
        }

        void think(
            std::size_t agentId,
            const network::Protocol::Response &response
        ) override;

    private:
        std::atomic_bool m_invoked{false};
    };
}
