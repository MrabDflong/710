#pragma once

#include "LogicGate.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Maximum external inputs supported by current UI.
inline constexpr std::size_t kMaxPuzzleInputs = 8;

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

    // Full explanation displayed in the briefing screen.
    std::string briefing;

    // Descriptions for external inputs (A to H).
    std::array<std::string, kMaxPuzzleInputs> inputDescriptions{};

    // What final output represents.
    std::string outputDescription;

    std::vector<GateType> availableGates;

    // Two inputs by default for existing level files.
    std::size_t inputCount{ 2 };

    // Expected outputs in binary input order:
    std::vector<bool> expectedOutputs;

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
