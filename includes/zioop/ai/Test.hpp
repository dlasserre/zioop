#pragma once

#include "Bot.hpp"

namespace ai {
    class Test : public Bot {
    public:
        using Bot::Bot;

        void run() override;

    protected:
        core::Direction chooseDirection() override;
    };
}
