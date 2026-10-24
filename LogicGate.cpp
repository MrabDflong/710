#include "LogicGate.h"

#include <stdexcept>

LogicGate::LogicGate(GateType type) : type_(type) {
}

std::size_t LogicGate::inputCount() const {
    switch (type_) {
    case GateType::And:
    case GateType::Or:
    case GateType::Nand:
    case GateType::Xor:
        return 2;

    case GateType::Not:
    case GateType::Wire:
        return 1;
    }

    throw std::logic_error("Unknown gate type.");
}

bool LogicGate::evaluate(std::span<const bool> inputs) const {
    if (inputs.size() != inputCount()) {
        throw std::invalid_argument("Incorrect number of inputs for logic gate.");
    }

    switch (type_) {
    case GateType::And:
        return inputs[0] && inputs[1];

    case GateType::Or:
        return inputs[0] || inputs[1];

    case GateType::Not:
        return !inputs[0];

    case GateType::Wire:
        return inputs[0];

    case GateType::Nand:
        return !(inputs[0] && inputs[1]);

    case GateType::Xor:
        return inputs[0] != inputs[1];
    }

    throw std::logic_error("Unknown gate type.");
}
