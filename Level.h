#pragma once

#include "LogicGate.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

struct Level {
    std::string id;
    std::string title;
    std::string objective;
    std::string hint;

    std::vector<GateType> availableGates;

    // Expected outputs for 00, 01, 10 & 11.
    std::array<bool, 4> expectedOutputs{};

    [[nodiscard]] bool allowsGate(GateType type) const {
        return std::find(
            availableGates.begin(),
            availableGates.end(),
            type
        ) != availableGates.end();
    }
};
