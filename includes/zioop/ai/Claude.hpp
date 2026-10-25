#pragma once

#include "zioop/ai/Bot.hpp"


namespace ai {

    class Claude : public Bot {
    public:
        using Bot::Bot;

    protected:

        [[nodiscard]] std::string_view getName() const override {
            return "Claude";
        }

        void think(
            std::size_t agentId,
            const network::Protocol::Response& response
        ) override;
    };

}
