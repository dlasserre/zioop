#include "zioop/ai/Claude.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <random>
#include <regex>
#include <ranges>
#include <limits>
#include <string>

namespace ai {

namespace {
    constexpr std::uint64_t MemoryLifetime = 80;
    constexpr std::uint64_t ShoutCooldown = 25;
    constexpr int ExplorationRadius = 6;
    constexpr std::size_t MaxTeamSize = 4;
    constexpr std::uint64_t InvocationWarmup = 5;
    constexpr std::uint64_t InvocationCooldown = 30;
    constexpr std::uint64_t InvocationBackoff = 60;

    constexpr std::array<const char *, 4> ResourceNames{"une pomme", "un gâteau", "de la viande", "de l'eau magique"};

    std::mt19937 &rng() {
        static thread_local std::mt19937 generator{std::random_device{}()};
        return generator;
    }

    std::optional<Claude::Point> readPoint(const nlohmann::json &value) {
        if (value.is_array() && value.size() == 2 && value[0].is_number_integer() && value[1].is_number_integer()) {
            return Claude::Point{value[0].get<int>(), value[1].get<int>()};
        }
        if (value.is_object() && value.contains("x") && value.contains("y") &&
            value["x"].is_number_integer() && value["y"].is_number_integer()) {
            return Claude::Point{value["x"].get<int>(), value["y"].get<int>()};
        }
        return std::nullopt;
    }

    std::optional<core::Direction> directionOf(const nlohmann::json &parameters) {
        const auto field = parameters.find("direction");
        if (field == parameters.end() || !field->is_number_unsigned()) {
            return std::nullopt;
        }
        return static_cast<core::Direction>(field->get<std::uint16_t>());
    }

    Claude::Point step(const Claude::Point &from, const core::Direction direction) {
        switch (direction) {
            case core::Direction::NORTH: return {from.first, from.second - 1};
            case core::Direction::SOUTH: return {from.first, from.second + 1};
            case core::Direction::EAST: return {from.first + 1, from.second};
            case core::Direction::WEST: return {from.first - 1, from.second};
        }
        return from;
    }

    int distance(const Claude::Point &a, const Claude::Point &b) {
        return std::abs(a.first - b.first) + std::abs(a.second - b.second);
    }

    std::string describe(const Claude::Point &point) {
        return std::format("({}, {})", point.first, point.second);
    }
}

Claude::Counts Claude::requirements(const Memory &memory) const {
    switch (memory.level) {
        case 1: return {20, 0, 4, 0};
        case 2: return {50, 8, 10, 0};
        default: return {};
    }
}

bool Claude::canPerformRitual(const Memory &memory) const {
    const auto needed = requirements(memory);
    if (memory.ritualBackoff > 0 || std::ranges::all_of(needed, [](const std::size_t value) { return value == 0; })) {
        return false;
    }
    for (std::size_t type = 0; type < ResourceKinds; ++type) {
        if (memory.inventory[type] < needed[type]) {
            return false;
        }
    }
    return true;
}

std::optional<int> Claude::wantedResourceAt(const Memory &memory, const Point &point) const {
    const auto cell = memory.cells.find(point);
    if (cell == memory.cells.end()) {
        return std::nullopt;
    }

    const auto needed = requirements(memory);
    const bool collectAnything = memory.ritualBackoff > 0 ||
                                 std::ranges::all_of(needed, [](const std::size_t value) { return value == 0; });

    std::optional<int> best;
    std::size_t bestDeficit = 0;
    for (std::size_t type = 0; type < ResourceKinds; ++type) {
        if (cell->second.resources[type] == 0 || memory.untakeable[type]) {
            continue;
        }
        const std::size_t deficit = needed[type] > memory.inventory[type] ? needed[type] - memory.inventory[type] : 0;
        if ((deficit > bestDeficit) || (collectAnything && !best)) {
            best = static_cast<int>(type);
            bestDeficit = deficit;
        }
    }
    return best;
}

void Claude::absorbLook(Memory &memory, const nlohmann::json &payload) {
    const auto cells = payload.find("cells");
    if (cells == payload.end() || !cells->is_array()) {
        return;
    }

    Point richest{};
    std::size_t richestCount = 0;

    for (const auto &cell: *cells) {
        if (!cell.is_object()) {
            continue;
        }
        const auto position = readPoint(cell.value("position", nlohmann::json{}));
        if (!position) {
            continue;
        }

        KnownCell known{.seenAt = memory.clock};
        if (const auto resources = cell.find("resources"); resources != cell.end() && resources->is_array()) {
            for (const auto &resource: *resources) {
                const int type = resource.is_object() ? resource.value("type", -1) : -1;
                if (type >= 0 && type < static_cast<int>(ResourceKinds)) {
                    ++known.resources[static_cast<std::size_t>(type)];
                }
            }
        }
        memory.cells[*position] = known;

        std::erase_if(memory.enemies, [&](const auto &entry) {
            return entry.second.position == *position && entry.second.seenAt < memory.clock;
        });
        if (const auto agents = cell.find("agents"); agents != cell.end() && agents->is_array()) {
            for (const auto &other: *agents) {
                if (!other.is_object() || !other.contains("id") || !other["id"].is_number_unsigned()) {
                    continue;
                }
                const auto id = other["id"].get<std::size_t>();
                if (isOwnAgent(id)) {
                    continue;
                }
                const int level = other.value("level", 1);
                memory.enemies[id] = {.position = *position, .level = level, .seenAt = memory.clock};
                if (level > memory.level && memory.clock - memory.lastShout > ShoutCooldown) {
                    memory.pendingShout = std::format("Attention, ennemi niveau {} en {},{}", level,
                                                      position->first, position->second);
                }
            }
        }

        memory.maxX = memory.maxX ? (std::max)(*memory.maxX, position->first) : memory.maxX;
        memory.maxY = memory.maxY ? (std::max)(*memory.maxY, position->second) : memory.maxY;

        std::size_t total = 0;
        for (const auto count: known.resources) {
            total += count;
        }
        if (memory.position && *position != *memory.position && total > richestCount) {
            richest = *position;
            richestCount = total;
        }
    }

    if (richestCount >= 2 && memory.clock - memory.lastShout > ShoutCooldown) {
        memory.pendingShout = std::format("Ressources en {},{}", richest.first, richest.second);
    }
}

bool Claude::absorbRitualEvent(Memory &memory, const nlohmann::json &payload) {
    if (!payload.is_object() || payload.value("type", std::string{}) != "Ritual") {
        return false;
    }
    memory.inRitual = false;
    memory.ritualLooks = 0;
    memory.needInventory = true;
    if (payload.contains("level") && payload["level"].is_number_integer()) {
        memory.level = payload["level"].get<int>();
        memory.pendingShout = std::format("Niveau {} atteint !", memory.level);
    }
    return true;
}

void Claude::absorbRumors(const std::size_t agentId, Memory &memory) {
    static const std::regex coordinates(R"((\d+)\s*,\s*(\d+))");

    const auto heard = takeHeard(agentId);
    if (!heard) {
        return;
    }

    std::smatch match;
    if (heard->message.find("ennemi") != std::string::npos || !std::regex_search(heard->message, match, coordinates)) {
        return;
    }

    const Point point{std::stoi(match[1].str()), std::stoi(match[2].str())};
    auto &cell = memory.cells[point];
    if (memory.clock - cell.seenAt > 5 || std::ranges::all_of(cell.resources, [](const std::size_t value) { return value == 0; })) {
        cell.resources.fill(1);
        cell.seenAt = memory.clock;
    }
    std::cout << "[" << agentId << "] Rumeur entendue : ressources en " << describe(point) << std::endl;
}

void Claude::remember(Memory &memory, const Decision &previous, const bool ok, const nlohmann::json &payload) {
    using MessageType = network::Protocol::MessageType;

    switch (previous.command) {
        case MessageType::MOVE: {
            if (ok) {
                if (const auto position = readPoint(payload.value("position", nlohmann::json{}))) {
                    memory.position = position;
                }
                break;
            }
            const auto direction = directionOf(previous.parameters);
            if (memory.position && direction) {
                if (*direction == core::Direction::EAST) {
                    memory.maxX = memory.position->first;
                } else if (*direction == core::Direction::SOUTH) {
                    memory.maxY = memory.position->second;
                }
                auto &blocked = memory.cells[step(*memory.position, *direction)];
                blocked.resources.fill(0);
                blocked.seenAt = memory.clock + MemoryLifetime;
            }
            memory.target.reset();
            break;
        }
        case MessageType::LOOK:
            if (ok) {
                absorbLook(memory, payload);
            }
            break;
        case MessageType::TAKE:
        case MessageType::INVENTORY: {
            const auto type = previous.parameters.value("type", -1);
            if (previous.command == MessageType::TAKE && memory.position) {
                auto &here = memory.cells[*memory.position];
                if (type >= 0 && type < static_cast<int>(ResourceKinds)) {
                    auto &count = here.resources[static_cast<std::size_t>(type)];
                    count = ok && count > 0 ? count - 1 : 0;
                }
            }
            if (!ok) {
                if (previous.command == MessageType::TAKE && type >= 0 && type < static_cast<int>(ResourceKinds) &&
                    payload.value("message", std::string{}) == "Type not found") {
                    memory.untakeable[static_cast<std::size_t>(type)] = true;
                    std::cout << "[" << previous.parameters.value("agentID", 0) << "] Type " << type
                              << " refuse par le serveur, je l'ignore desormais." << std::endl;
                }
                break;
            }
            if (const auto items = payload.find("resources"); items != payload.end() && items->is_array()) {
                Counts inventory{};
                for (const auto &item: *items) {
                    if (!item.is_object()) {
                        continue;
                    }
                    const int itemType = item.value("type", -1);
                    if (itemType >= 0 && itemType < static_cast<int>(ResourceKinds)) {
                        inventory[static_cast<std::size_t>(itemType)] = item.value("quantity", std::size_t{0});
                    }
                }
                memory.inventory = inventory;
            }
            if (memory.ritualBackoff > 0 && previous.command == MessageType::TAKE && ok) {
                --memory.ritualBackoff;
            }
            break;
        }
        case MessageType::ATTACK:
            rememberAttack(memory, previous, ok, payload);
            break;
        case MessageType::INVOCATION:
            rememberInvocation(memory, previous, ok, payload);
            break;
        case MessageType::RITUAL:
            if (!ok) {
                memory.ritualBackoff = 6;
                break;
            }
            if (!absorbRitualEvent(memory, payload)) {
                memory.inRitual = true;
                memory.ritualLooks = 0;
                memory.plannedMove.reset();
                memory.target.reset();
            }
            break;
        default:
            break;
    }
}

std::optional<Claude::Point> Claude::chooseTarget(Memory &memory) const {
    if (!memory.position) {
        return std::nullopt;
    }
    const Point here = *memory.position;

    std::optional<Point> best;
    float bestScore = 0.0f;
    for (const auto &[point, cell]: memory.cells) {
        if (point == here || memory.clock > cell.seenAt + MemoryLifetime) {
            continue;
        }
        if (!wantedResourceAt(memory, point)) {
            continue;
        }
        std::size_t total = 0;
        for (const auto count: cell.resources) {
            total += count;
        }
        const float score = static_cast<float>(total) / static_cast<float>(distance(here, point) + 1);
        if (score > bestScore) {
            bestScore = score;
            best = point;
        }
    }
    if (best) {
        return best;
    }

    std::uniform_real_distribution<float> jitter(0.0f, 0.3f);
    for (int dy = -ExplorationRadius; dy <= ExplorationRadius; ++dy) {
        for (int dx = -ExplorationRadius; dx <= ExplorationRadius; ++dx) {
            const Point point{here.first + dx, here.second + dy};
            const int away = distance(here, point);
            if (away == 0 || away > ExplorationRadius || point.first < 0 || point.second < 0 ||
                (memory.maxX && point.first > *memory.maxX) || (memory.maxY && point.second > *memory.maxY)) {
                continue;
            }
            const auto cell = memory.cells.find(point);
            const float staleness = cell == memory.cells.end()
                                        ? 150.0f
                                        : static_cast<float>(memory.clock > cell->second.seenAt ? memory.clock - cell->second.seenAt : 0);
            const float score = staleness / static_cast<float>(away + 2) + jitter(rng());
            if (score > bestScore) {
                bestScore = score;
                best = point;
            }
        }
    }
    return best;
}

void Claude::rememberAttack(Memory &memory, const Decision &previous, const bool ok, const nlohmann::json &payload) {
    const auto target = previous.parameters.value("targetId", std::size_t{0});
    const auto self = previous.parameters.value("agentID", std::size_t{0});

    if (ok) {
        if (payload.value("killed", false)) {
            memory.enemies.erase(target);
            memory.pendingShout = std::format("Ennemi #{} vaincu !", target);
            std::cout << "[" << self << "] Ennemi #" << target << " vaincu." << std::endl;
        } else {
            std::cout << "[" << self << "] Touche l'ennemi #" << target << "." << std::endl;
        }
        return;
    }

    const auto reason = payload.value("message", std::string{});
    if (reason.find("too strong") != std::string::npos) {
        memory.spared[target] = memory.clock + 60;
        if (const auto enemy = memory.enemies.find(target); enemy != memory.enemies.end()) {
            enemy->second.level = (std::max)(enemy->second.level, memory.level + 1);
        }
    } else if (reason.find("yourself") != std::string::npos) {
        memory.spared[target] = (std::numeric_limits<std::uint64_t>::max)();
        memory.enemies.erase(target);
    } else if (reason.find("Ritual") == std::string::npos) {
        memory.enemies.erase(target);
    }
}

bool Claude::wantsReinforcement(const Memory &memory) const {
    return memory.clock > InvocationWarmup &&
           memory.clock >= memory.nextInvocation &&
           m_memory.size() + m_pendingInvocations < MaxTeamSize;
}

void Claude::rememberInvocation(Memory &memory, const Decision &previous, const bool ok, const nlohmann::json &payload) {
    const auto self = previous.parameters.value("agentID", std::size_t{0});

    if (m_pendingInvocations > 0) {
        --m_pendingInvocations;
    }

    if (!ok) {
        memory.nextInvocation = memory.clock + InvocationBackoff;
        std::cerr << "[" << self << "] Invocation refusee : " << payload.value("message", std::string{}) << std::endl;
        return;
    }

    memory.nextInvocation = memory.clock + InvocationCooldown;
    const auto spawned = payload.value("spawnedAgentId", std::size_t{0});
    if (spawned != 0) {
        memory.pendingShout = std::format("Renfort #{} invoque", spawned);
    }
}

std::optional<std::size_t> Claude::enemyToStrike(const Memory &memory) const {
    if (!memory.position) {
        return std::nullopt;
    }

    std::optional<std::size_t> best;
    int bestLevel = -1;
    for (const auto &[id, enemy]: memory.enemies) {
        if (enemy.position != *memory.position || memory.clock > enemy.seenAt + 2 || enemy.level > memory.level) {
            continue;
        }
        if (const auto spared = memory.spared.find(id); spared != memory.spared.end() && spared->second > memory.clock) {
            continue;
        }
        if (enemy.level > bestLevel) {
            best = id;
            bestLevel = enemy.level;
        }
    }
    return best;
}

std::optional<Claude::Point> Claude::nearestThreat(const Memory &memory) const {
    if (!memory.position) {
        return std::nullopt;
    }

    std::optional<Point> closest;
    int closestDistance = 2;
    for (const auto &enemy: memory.enemies | std::views::values) {
        if (enemy.level <= memory.level || memory.clock > enemy.seenAt + 3) {
            continue;
        }
        const int away = distance(*memory.position, enemy.position);
        if (away < closestDistance) {
            closest = enemy.position;
            closestDistance = away;
        }
    }
    return closest;
}

std::optional<std::pair<std::size_t, Claude::Point>> Claude::nearestPrey(const Memory &memory) const {
    if (!memory.position) {
        return std::nullopt;
    }

    std::optional<std::pair<std::size_t, Point>> closest;
    int closestDistance = 5;
    for (const auto &[id, enemy]: memory.enemies) {
        if (enemy.level > memory.level || memory.clock > enemy.seenAt + 10) {
            continue;
        }
        if (const auto spared = memory.spared.find(id); spared != memory.spared.end() && spared->second > memory.clock) {
            continue;
        }
        const int away = distance(*memory.position, enemy.position);
        if (away > 0 && away < closestDistance) {
            closest = std::make_pair(id, enemy.position);
            closestDistance = away;
        }
    }
    return closest;
}

std::optional<core::Direction> Claude::stepAway(const Memory &memory, const Point &danger) const {
    if (!memory.position) {
        return std::nullopt;
    }

    std::optional<core::Direction> best;
    int bestDistance = -1;
    for (const auto direction: directions) {
        const Point next = step(*memory.position, direction);
        if (next.first < 0 || next.second < 0 ||
            (memory.maxX && next.first > *memory.maxX) || (memory.maxY && next.second > *memory.maxY)) {
            continue;
        }
        const int away = distance(next, danger);
        if (away > bestDistance) {
            best = direction;
            bestDistance = away;
        }
    }
    return best;
}

std::optional<core::Direction> Claude::stepToward(const Memory &memory, const Point &target) const {
    if (!memory.position) {
        return std::nullopt;
    }
    const int dx = target.first - memory.position->first;
    const int dy = target.second - memory.position->second;

    const auto horizontal = dx > 0 ? core::Direction::EAST : core::Direction::WEST;
    const auto vertical = dy > 0 ? core::Direction::SOUTH : core::Direction::NORTH;

    if (dx == 0 && dy == 0) {
        return std::nullopt;
    }
    if (dx == 0) {
        return vertical;
    }
    if (dy == 0) {
        return horizontal;
    }
    return std::abs(dx) >= std::abs(dy) ? horizontal : vertical;
}

void Claude::decide(
    const std::size_t agentId,
    Memory &memory,
    const std::optional<network::Protocol::MessageType> previous
) {
    using MessageType = network::Protocol::MessageType;

    absorbRumors(agentId, memory);

    if (memory.inRitual) {
        memory.plannedMove.reset();
        if (memory.ritualLooks < directions.size()) {
            look(agentId, directions[memory.ritualLooks++]);
            return;
        }
        waitForRitual(agentId);
        return;
    }

    if (memory.needInventory) {
        memory.needInventory = false;
        inventory(agentId);
        return;
    }

    if (memory.pendingShout) {
        broadcast(agentId, *memory.pendingShout);
        memory.pendingShout.reset();
        memory.lastShout = memory.clock;
        return;
    }

    if (const auto danger = nearestThreat(memory)) {
        if (const auto escape = stepAway(memory, *danger)) {
            memory.target.reset();
            if (previous == MessageType::LOOK && memory.plannedMove) {
                const auto direction = *memory.plannedMove;
                memory.plannedMove.reset();
                move(agentId, direction);
                return;
            }
            std::cout << "[" << agentId << "] Ennemi trop fort en " << describe(*danger) << ", je fuis." << std::endl;
            memory.plannedMove = *escape;
            look(agentId, *escape);
            return;
        }
    }

    if (const auto target = enemyToStrike(memory)) {
        memory.plannedMove.reset();
        attack(agentId, *target);
        return;
    }

    if (wantsReinforcement(memory)) {
        ++m_pendingInvocations;
        std::cout << "[" << agentId << "] J'invoque un renfort (equipe " << m_memory.size() << "/" << MaxTeamSize
                  << ")." << std::endl;
        invocation(agentId);
        return;
    }

    if (canPerformRitual(memory)) {
        std::cout << "[" << agentId << "] J'ai tout ce qu'il faut, je lance le rituel." << std::endl;
        ritual(agentId);
        return;
    }

    if (memory.position) {
        if (const auto type = wantedResourceAt(memory, *memory.position)) {
            std::cout << "[" << agentId << "] Je ramasse " << ResourceNames[static_cast<std::size_t>(*type)]
                      << " en " << describe(*memory.position) << std::endl;
            take(agentId, *type);
            return;
        }
    }

    if (previous == MessageType::LOOK && memory.plannedMove) {
        const auto direction = *memory.plannedMove;
        memory.plannedMove.reset();
        move(agentId, direction);
        return;
    }
    memory.plannedMove.reset();

    if (memory.target && memory.position && *memory.target == *memory.position) {
        memory.target.reset();
    }
    if (!memory.target || !wantedResourceAt(memory, *memory.target)) {
        memory.target = chooseTarget(memory);
        if (memory.target && memory.position) {
            const bool resources = wantedResourceAt(memory, *memory.target).has_value();
            std::cout << "[" << agentId << "] Objectif : "
                      << (resources ? "ressources" : "exploration") << " en " << describe(*memory.target)
                      << " (distance " << distance(*memory.position, *memory.target) << ")" << std::endl;
        }
    }

    if (const auto prey = nearestPrey(memory); prey && memory.position) {
        const bool resourceTarget = memory.target && wantedResourceAt(memory, *memory.target).has_value();
        if (!resourceTarget || distance(*memory.position, prey->second) <= distance(*memory.position, *memory.target)) {
            if (memory.target != prey->second) {
                std::cout << "[" << agentId << "] Je chasse l'ennemi #" << prey->first
                          << " en " << describe(prey->second) << std::endl;
            }
            memory.target = prey->second;
        }
    }

    auto direction = memory.target ? stepToward(memory, *memory.target) : std::nullopt;
    if (!direction) {
        std::uniform_int_distribution<std::size_t> pick(0, directions.size() - 1);
        direction = directions[pick(rng())];
    }

    memory.plannedMove = *direction;
    look(agentId, *direction);
}

void Claude::think(
    const std::size_t agentId,
    const network::Protocol::Response &response
) {
    auto &memory = m_memory[agentId];
    ++memory.clock;

    const auto payload = nlohmann::json::parse(response.payload, nullptr, false);
    const bool ok = response.header.type == network::Protocol::ResponseType::OK;
    const auto previously = getPreviously(agentId);

    if (!previously) {
        if (payload.is_object()) {
            memory.position = readPoint(payload.value("position", nlohmann::json{}));
        }
        decide(agentId, memory, std::nullopt);
        return;
    }

    if (!ok) {
        std::cerr << "[" << agentId << "] KO: " << response.payload << std::endl;
    }

    try {
        if (previously->command != network::Protocol::MessageType::RITUAL && absorbRitualEvent(memory, payload)) {
            std::cout << "[" << agentId << "] Rituel termine, niveau " << memory.level << std::endl;
        } else {
            remember(memory, *previously, ok, payload.is_object() ? payload : nlohmann::json::object());
        }
        if (const auto event = takeRitualEvent(agentId)) {
            absorbRitualEvent(memory, nlohmann::json::parse(*event, nullptr, false));
        }
    } catch (const nlohmann::json::exception &error) {
        std::cerr << "[" << agentId << "] Unreadable response: " << error.what() << std::endl;
    }

    decide(agentId, memory, previously->command);
}

}
