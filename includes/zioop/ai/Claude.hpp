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
            const network::Protocol::Response& response
        ) override;
    };

}
