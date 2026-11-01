#include "zioop/network/EventDispatcher.hpp"

#include <utility>
#include <vector>
#include <variant>

#include "zioop/network/Protocol.hpp"
#include "zioop/network/Serialization.hpp"

namespace network {
    EventDispatcher::EventDispatcher(Sender sender) : m_sender(std::move(sender)) {
    }

    template <typename T>
    void EventDispatcher::send(const T &event) const {
        m_sender(
            event.base.clientId,
            Protocol::encode(
                Protocol::ResponseType::EVENT,
                serializeEvent(event).dump()
            )
        );
    }

    void EventDispatcher::dispatch(const std::vector<core::Event> &events) const {
        for (const auto &event: events) {
            std::visit([this](const auto &e) { send(e); }, event);
        }
    }
}
