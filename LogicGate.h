#pragma once

#include <cstddef>
#include <span>

enum class GateType {
    And,
    Or,
    Not
};

class LogicGate {
public:
    explicit LogicGate(GateType type);

    [[nodiscard]] std::size_t inputCount() const;
    [[nodiscard]] bool evaluate(std::span<const bool> inputs) const;

private:
    GateType type_;
};