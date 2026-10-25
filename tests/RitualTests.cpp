#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "zioop/core/Game.hpp"

namespace {
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;
    using Result = core::RitualStartResult;
    constexpr auto Fruit = core::ResourceType::APPLE;

    void check(const bool condition, const std::string_view message) {
        if (!condition) {
            throw std::runtime_error(std::string(message));
        }
    }

    std::size_t fruitCount(const std::vector<core::InventoryResource> &inventory) {
        for (const auto &resource: inventory) {
            if (resource.type == Fruit) {
                return resource.quantity;
            }
        }
        return 0;
    }

    core::Matrix makeMatrix() {
        core::Matrix matrix(3, 3);
        core::Agent seeder;
        check(matrix.addAgent(seeder).has_value(), "place seeder");
        auto *cell = seeder.getCell();
        while (cell->take(seeder, Fruit)) {}
        for (int i = 0; i < 30; ++i) {
            cell->addResource(Fruit);
        }
        matrix.removeAgent(seeder);
        return matrix;
    }

    void testAgentDeadline() {
        auto matrix = makeMatrix();
        core::Agent agent;
        check(matrix.addAgent(agent).has_value(), "place agent");
        for (int i = 0; i < 12; ++i) {
            check(agent.take(Fruit), "collect fruit");
        }

        const auto now = Clock::time_point{};
        check(agent.startRitual(60s, now), "start ritual");
        check(fruitCount(agent.getInventory()) == 2, "consume exactly ten fruits");
        check(agent.getLevel() == 1, "no level up at start");
        check(!agent.startRitual(1s, now), "reject duplicate ritual");
        check(fruitCount(agent.getInventory()) == 2, "duplicate must not consume fruit");
        check(!agent.take(Fruit), "ritual blocks collection");
        check(!agent.move(matrix, core::Direction::WEST), "ritual blocks movement");
        check(agent.look(matrix, core::Direction::WEST).has_value(), "look remains available");
        check(!agent.completeRitual(now + 60s - 1ns), "do not finish before deadline");
        check(agent.getLevel() == 1, "keep old level before deadline");
        check(agent.completeRitual(now + 60s), "finish exactly at deadline");
        check(agent.getLevel() == 2 && !agent.isRitualInProgress(), "level up and unlock");
        check(!agent.completeRitual(now + 1h), "complete only once");
        check(agent.take(Fruit), "collection resumes");
        check(agent.move(matrix, core::Direction::WEST).has_value(), "movement resumes");
    }

    void collect(core::Game &game, const std::size_t clientId,
                 const std::size_t agentId, const int count) {
        for (int i = 0; i < count; ++i) {
            const auto result = game.take(clientId, agentId, Fruit);
            check(result && result->taken, "collect through Game");
        }
    }

    void testGameRitual() {
        core::Game game(makeMatrix());
        const auto agent = game.initClient(10);
        const auto other = game.initClient(20);
        check(agent.has_value() && other.has_value(), "place game agents");
        const auto id = agent->agentId;

        check(!game.ritual(10, 0), "unknown agent");
        collect(game, 10, id, 9);
        check(!game.ritual(20, id), "reject wrong owner");
        check(game.ritual(10, id) == Result::NOT_ENOUGH_RESOURCES, "need ten fruits");
        check(fruitCount(*game.inventory(10, id)) == 9, "failed start preserves inventory");

        collect(game, 10, id, 2);
        const auto before = Clock::now();
        check(game.ritual(10, id) == Result::STARTED, "start with sufficient resources");
        const auto after = Clock::now();
        check(fruitCount(*game.inventory(10, id)) == 1, "consume cost once");
        check(game.ritual(10, id) == Result::ALREADY_IN_PROGRESS, "duplicate status");
        check(!game.move(10, id, core::Direction::WEST), "Game blocks movement");
        const auto take = game.take(10, id, Fruit);
        check(take && !take->taken && fruitCount(take->inventory) == 1,
              "Game blocks collection without consuming fruit");
        check(game.look(10, id, core::Direction::WEST).has_value(), "Game allows look");
        check(game.move(20, other->agentId, core::Direction::EAST).has_value(),
              "other agents remain active");
        check(game.tick(before + 60s - 1ns).empty(), "level 2 needs sixty seconds");

        const auto completed = game.tick(after + 60s);
        check(completed.size() == 1, "one completion");
        check(completed[0].clientId == 10 && completed[0].agentId == id &&
              completed[0].level == 2, "completion identifies owner, agent and level");
        check(game.tick(after + 120s).empty(), "no repeated completion");

        collect(game, 10, id, 9);
        const auto beforeSecond = Clock::now();
        check(game.ritual(10, id) == Result::STARTED, "start second ritual");
        const auto afterSecond = Clock::now();
        check(game.tick(beforeSecond + 90s - 1ns).empty(), "level 3 needs ninety seconds");
        const auto second = game.tick(afterSecond + 90s);
        check(second.size() == 1 && second[0].level == 3, "reach level 3");
    }

    void testDisconnection() {
        core::Game game(makeMatrix());
        const auto agent = game.initClient(10);
        check(agent.has_value(), "place disconnecting agent");
        collect(game, 10, agent->agentId, 10);
        check(game.ritual(10, agent->agentId) == Result::STARTED, "start before disconnect");
        game.removeClientAgent(10);
        check(game.tick(Clock::now() + 1h).empty(), "removed agent cannot complete ritual");
        check(!game.inventory(10, agent->agentId), "removed inventory is inaccessible");
        const auto replacement = game.initClient(20);
        check(replacement.has_value(), "disconnection frees cell");
        check(game.tick(Clock::now() + 2h).empty(), "no completion for replacement agent");
    }
}

int main() {
    try {
        testAgentDeadline();
        testGameRitual();
        testDisconnection();
        std::cout << "Ritual tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
