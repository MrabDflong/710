#pragma once

#include "LogicGate.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Describes one gate position on circuit board.
struct SocketDefinition {
    std::string id;

    float x{};
    float y{};

    // Sources are "A", "B", or previous socket IDs.
    std::vector<std::string> sources;
};

// Describes one externally loaded work order.
struct Level {
    std::string id;
    std::string title;
    std::string objective;
    std::string hint;

    std::vector<GateType> availableGates;

    // Expected outputs for 00, 01, 10 and 11.
    std::array<bool, 4> expectedOutputs{};

    std::vector<SocketDefinition> sockets;

    // ID of socket connected to Y.
    std::string outputSocket;

    // Optional target for efficiency award. WIRE does not contribute to gate count.
    std::optional<std::size_t> optimalGateCount;

    // Component awarded for completing work order.
    std::optional<GateType> certifies;

    // Narrative message displayed when entering level.
    std::string arrivalMessage;

    [[nodiscard]] bool allowsGate(GateType type) const {
        return std::find(
            availableGates.begin(),
            availableGates.end(),
            type
        ) != availableGates.end();
    }
};
