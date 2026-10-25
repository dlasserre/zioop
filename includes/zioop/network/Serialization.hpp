#pragma once

#include <nlohmann/json.hpp>

#include "zioop/core/Inventory.hpp"
#include "zioop/core/LookResult.hpp"
#include "zioop/core/MatrixSnapshot.hpp"

namespace network {
    [[nodiscard]]
    nlohmann::json serializeHello(const core::HelloResult &hello);

    [[nodiscard]]
    nlohmann::json serializeLook(const core::LookResult &observation);

    [[nodiscard]]
    nlohmann::json serializePosition(const core::MoveResult &move);

    [[nodiscard]]
    nlohmann::json serializeMatrix(const core::MatrixResult &snapshot);

    [[nodiscard]]
    nlohmann::json serializeInventory(
        const core::InventoryResult &inventory
    );
}
