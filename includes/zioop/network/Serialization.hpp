#pragma once

#include <nlohmann/json.hpp>

#include "zioop/core/Inventory.hpp"
#include "zioop/core/LookResult.hpp"
#include "zioop/core/MatrixSnapshot.hpp"

namespace network {
    [[nodiscard]]
    nlohmann::json serializeLook(const core::LookResult &observation);

    [[nodiscard]]
    nlohmann::json serializePosition(const core::Position &position);

    [[nodiscard]]
    nlohmann::json serializeMatrix(const core::MatrixSnapshot &snapshot);

    [[nodiscard]]
    nlohmann::json serializeInventory(
        const std::vector<core::InventoryResource> &inventory
    );
}
