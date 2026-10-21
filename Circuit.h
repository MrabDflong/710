#pragma once

#include "LogicGate.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

class Circuit {
public:
    using GateId = std::size_t;

    // Identifies where a signal originates.
    struct SignalSource {
        enum class Kind {
            Input,
            Gate
        };

        Kind kind;
        std::size_t index;

        [[nodiscard]] static SignalSource input(std::size_t index) {
            return { Kind::Input, index };
        }

        [[nodiscard]] static SignalSource gate(GateId id) {
            return { Kind::Gate, id };
        }
    };

    explicit Circuit(std::size_t inputCount);

    [[nodiscard]] GateId addGate(
        GateType type,
        std::vector<SignalSource> sources
    );

    void setOutput(SignalSource source);

    [[nodiscard]] bool evaluate(std::span<const bool> inputs) const;

private:
    struct GateNode {
        LogicGate gate;
        std::vector<SignalSource> sources;
    };

    void validateSource(
        SignalSource source,
        std::size_t availableGates
    ) const;

    std::size_t inputCount_;
    std::vector<GateNode> gates_;
    std::optional<SignalSource> output_;
};
