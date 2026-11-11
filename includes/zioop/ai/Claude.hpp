#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
            int health{100};
            std::uint64_t seenAt{0};
        };

        struct Request {
            std::size_t type{0};
            Point where{};
            std::uint64_t heardAt{0};
        };

        struct Memory {
            std::map<std::size_t, Enemy> enemies;
            std::map<std::size_t, Request> requests;
            std::uint64_t lastRequest{0};
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
            std::uint64_t nextInvocation{0};
            int health{100};
            std::uint64_t hurtAt{0};
            std::uint64_t fleeUntil{0};
            std::optional<Point> fleeFrom;
            std::uint64_t nextGive{0};
        };

        void remember(Memory &memory, const Decision &previous, bool ok, const nlohmann::json &payload);

        void absorbLook(Memory &memory, const nlohmann::json &payload);

        static bool absorbRitualEvent(Memory &memory, const nlohmann::json &payload);

        static void absorbRotten(Memory &memory, const std::vector<int> &lost);

        void absorbRumors(std::size_t agentId, Memory &memory);

        void decide(std::size_t agentId, Memory &memory, std::optional<network::Protocol::MessageType> previous);

        [[nodiscard]] Counts requirements(const Memory &memory) const;

        [[nodiscard]] bool canPerformRitual(const Memory &memory) const;

        [[nodiscard]] std::optional<int> wantedResourceAt(const Memory &memory, const Point &point) const;

        [[nodiscard]] std::optional<Point> chooseTarget(Memory &memory) const;

        [[nodiscard]] std::optional<std::size_t> enemyToStrike(const Memory &memory) const;

        [[nodiscard]] std::optional<Point> losingFight(const Memory &memory) const;

        static void feelHealth(Memory &memory, int health);

        [[nodiscard]] std::optional<std::pair<std::size_t, Point>> nearestPrey(const Memory &memory) const;

        [[nodiscard]] std::optional<core::Direction> stepAway(const Memory &memory, const Point &danger) const;

        void rememberAttack(Memory &memory, const Decision &previous, bool ok, const nlohmann::json &payload);

        [[nodiscard]] std::optional<core::Direction> stepToward(const Memory &memory, const Point &target) const;

        [[nodiscard]] bool wantsReinforcement(const Memory &memory) const;

        [[nodiscard]] Counts deficit(const Memory &memory) const;

        [[nodiscard]] Counts surplus(const Memory &memory) const;

        [[nodiscard]] std::optional<std::pair<std::size_t, int>> donation(std::size_t agentId, const Memory &memory) const;

        [[nodiscard]] std::optional<Point> allyInNeed(std::size_t agentId, const Memory &memory) const;

        [[nodiscard]] std::optional<std::string> resourceRequest(const Memory &memory) const;

        void rememberGive(Memory &memory, const Decision &previous, bool ok, const nlohmann::json &payload);

        void rememberInvocation(Memory &memory, const Decision &previous, bool ok, const nlohmann::json &payload);

        std::unordered_map<std::size_t, Memory> m_memory;
        std::size_t m_pendingInvocations{0};
    };

}
