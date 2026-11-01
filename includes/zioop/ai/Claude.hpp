#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "zioop/ai/Bot.hpp"


namespace ai {

    class Claude : public Bot {
    public:
        using Bot::Bot;

        static constexpr std::size_t ResourceKinds = 4;

        using Point = std::pair<int, int>;
        using Counts = std::array<std::size_t, ResourceKinds>;

    protected:

        [[nodiscard]] std::string_view getName() const override {
            return "Claude";
        }

        void think(
            std::size_t agentId,
            const network::Protocol::Response& response
        ) override;

    private:
        struct KnownCell {
            Counts resources{};
            std::uint64_t seenAt{0};
        };

        struct Enemy {
            Point position;
            int level{1};
            std::uint64_t seenAt{0};
        };

        struct Memory {
            std::map<std::size_t, Enemy> enemies;
            std::map<std::size_t, std::uint64_t> spared;
            std::optional<Point> position;
            std::map<Point, KnownCell> cells;
            Counts inventory{};
            int level{1};
            std::optional<int> maxX;
            std::optional<int> maxY;
            std::optional<core::Direction> plannedMove;
            std::optional<Point> target;
            std::uint64_t clock{0};
            std::uint64_t lastShout{0};
            int ritualBackoff{0};
            bool needInventory{false};
            std::array<bool, ResourceKinds> untakeable{};
            bool inRitual{false};
            std::size_t ritualLooks{0};
            std::optional<std::string> pendingShout;
        };

        void remember(Memory &memory, const Decision &previous, bool ok, const nlohmann::json &payload);

        void absorbLook(Memory &memory, const nlohmann::json &payload);

        static bool absorbRitualEvent(Memory &memory, const nlohmann::json &payload);

        void absorbRumors(std::size_t agentId, Memory &memory);

        void decide(std::size_t agentId, Memory &memory, std::optional<network::Protocol::MessageType> previous);

        [[nodiscard]] Counts requirements(const Memory &memory) const;

        [[nodiscard]] bool canPerformRitual(const Memory &memory) const;

        [[nodiscard]] std::optional<int> wantedResourceAt(const Memory &memory, const Point &point) const;

        [[nodiscard]] std::optional<Point> chooseTarget(Memory &memory) const;

        [[nodiscard]] std::optional<std::size_t> enemyToStrike(const Memory &memory) const;

        [[nodiscard]] std::optional<Point> nearestThreat(const Memory &memory) const;

        [[nodiscard]] std::optional<std::pair<std::size_t, Point>> nearestPrey(const Memory &memory) const;

        [[nodiscard]] std::optional<core::Direction> stepAway(const Memory &memory, const Point &danger) const;

        void rememberAttack(Memory &memory, const Decision &previous, bool ok, const nlohmann::json &payload);

        [[nodiscard]] std::optional<core::Direction> stepToward(const Memory &memory, const Point &target) const;

        std::unordered_map<std::size_t, Memory> m_memory;
    };

}
