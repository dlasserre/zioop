#pragma once

#include <atomic>
#include <chrono>

namespace core {
    enum class ResourceType {
        APPLE,
        CAKE,
        MEAT,
        MAGIC_WATER
    };

    class Resource {
    public:
        explicit Resource(ResourceType type);

        void startRotting();

        [[nodiscard]] ResourceType getType() const {
            return m_type;
        }

        [[nodiscard]] int getRotting() const {
            return m_rotting.load();
        }

        virtual ~Resource() = default;

        [[nodiscard]]
        virtual std::string_view getName() const {
            switch (m_type) {
                case ResourceType::APPLE: return "Pomme";
                case ResourceType::CAKE: return "Gateau";
                case ResourceType::MEAT: return "Viande";
                case ResourceType::MAGIC_WATER: return "Eau magique";
            }

            return "Inconnue";
        }

    protected:
        virtual void decaying() = 0;

        std::atomic_int m_rotting{0};

        std::chrono::milliseconds m_decayInterval{3000};
        std::chrono::steady_clock::time_point m_nextDecay;

        ResourceType m_type;
    };
}
