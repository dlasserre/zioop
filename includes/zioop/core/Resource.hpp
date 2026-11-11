#pragma once

#include <chrono>
#include <array>
#include <string_view>

namespace core {
    enum class ResourceType { APPLE, CAKE, MEAT, MAGIC_WATER };

    constexpr std::array types{
        ResourceType::APPLE,
        ResourceType::CAKE,
        ResourceType::MAGIC_WATER,
        ResourceType::MEAT
    };

    inline constexpr std::size_t RESOURCE_KIND = 4;

    using ResourceCount = std::array<std::size_t, RESOURCE_KIND>;

    struct ResourceDef {
        std::string_view name;
        std::chrono::seconds lifetime;
    };

    constexpr const ResourceDef &def(const ResourceType type) {
        switch (type) {
            case ResourceType::APPLE: {
                static constexpr ResourceDef d{
                    .name = "Pomme",
                    .lifetime = std::chrono::seconds{300}
                };
                return d;
            }
            case ResourceType::CAKE: {
                static constexpr ResourceDef d{
                    .name = "Gateau",
                    .lifetime = std::chrono::seconds{120}
                };
                return d;
            }
            case ResourceType::MEAT: {
                static constexpr ResourceDef d{
                    .name = "Viande",
                    .lifetime = std::chrono::seconds{60}
                };
                return d;
            }
            case ResourceType::MAGIC_WATER: {
                static constexpr ResourceDef d{
                    .name = "Eau magique",
                    .lifetime = std::chrono::seconds{0}
                };
                return d;
            }
        }

        static constexpr ResourceDef unknown{
            .name = "Inconnue", .lifetime = std::chrono::seconds{0}
        };

        return unknown;
    }

    class Resource {
    public:
        using Clock = std::chrono::steady_clock;

        explicit Resource(ResourceType type);

        void startRotting(Clock::time_point now);

        [[nodiscard]] ResourceType type() const { return m_type; }
        [[nodiscard]] bool isRotten(const Clock::time_point now) const { return now > m_expiresAt; }
        [[nodiscard]] std::string_view name() const { return def(m_type).name; }

    protected:
        ResourceType m_type;
        Clock::time_point m_expiresAt;
    };
}
