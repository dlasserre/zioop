#pragma once

#include <iostream>
#include <cstddef>
#include <functional>
#include <vector>

#include "../core/Event.hpp"

namespace network {

    class EventDispatcher {
    public:

        using Sender = std::function<void(
            std::size_t clientId,
            const std::vector<std::byte> &frame
        )>;

        explicit EventDispatcher(Sender sender);

        void dispatch(const std::vector<core::Event> &events) const;

    private:
        template <typename T>
        void send(const T &event) const;

        Sender m_sender;
    };

}
