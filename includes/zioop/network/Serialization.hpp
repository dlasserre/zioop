#pragma once

#include <nlohmann/json.hpp>

#include "zioop/core/Event.hpp"
#include "zioop/core/Inventory.hpp"
#include "zioop/core/Results.hpp"
#include "zioop/core/MatrixSnapshot.hpp"

namespace network {

    [[nodiscard]]
    nlohmann::json serializeResource(const core::ObservedResource &resource);

    [[nodiscard]]
    nlohmann::json serializeHello(const core::HelloResult &hello);

    [[nodiscard]]
    nlohmann::json serializeLook(const core::LookResult &observation);

    [[nodiscard]]
    nlohmann::json serializePosition(const core::MoveResult &move);

    [[nodiscard]]
    nlohmann::json serializeMatrix(const core::MatrixResult &snapshot);

    [[nodiscard]]
    nlohmann::json serializeInventory(const core::InventoryResult &inventory);

    [[nodiscard]]
    nlohmann::json serializeAttack(const core::AttackResponse &attack);

    [[nodiscard]]
    nlohmann::json serializeRitual(const core::RitualResult &ritual);

    [[nodiscard]]
    nlohmann::json serializeBroadcast(const core::BroadcastResult &broadcast);

    [[nodiscard]]
    nlohmann::json serializeError(const core::ErrorResult &error);


    // Overload events
    [[nodiscard]]
    nlohmann::json serializeEvent(const core::Message &event);

    [[nodiscard]]
    nlohmann::json serializeEvent(const core::Died &event);

    [[nodiscard]]
    nlohmann::json serializeEvent(const core::RitualCompleted &event);
}
