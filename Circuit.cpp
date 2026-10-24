#include "Circuit.h"

#include <array>
#include <stdexcept>
#include <utility>

Circuit::Circuit(std::size_t inputCount) : inputCount_(inputCount) {

    if (inputCount_ == 0) {
        throw std::invalid_argument(
            "A circuit must have at least one input."
        );
    }
}

void Circuit::validateSource(
    SignalSource source,
    std::size_t availableGates
) const {
    switch (source.kind) {
    case SignalSource::Kind::Input:
        if (source.index >= inputCount_) {
            throw std::out_of_range(
                "Circuit input does not exist."
            );
        }
        return;

    case SignalSource::Kind::Gate:
        if (source.index >= availableGates) {
            throw std::out_of_range(
                "Referenced gate does not exist or has not been added yet."
            );
        }
        return;
    }

    throw std::invalid_argument("Unknown signal source.");
}

Circuit::GateId Circuit::addGate(
    GateType type,
    std::vector<SignalSource> sources
) {
    LogicGate gate{ type };

    if (sources.size() != gate.inputCount()) {
        throw std::invalid_argument(
            "Incorrect number of connections for gate."
        );
    }

    // A new gate may only use external inputs or gates that already exist.
    for (const auto& source : sources) {
        validateSource(source, gates_.size());
    }

    gates_.push_back({
        std::move(gate),
        std::move(sources)
    });

    return gates_.size() - 1;
}

void Circuit::setOutput(SignalSource source) {
    validateSource(source, gates_.size());
    output_ = source;
}

bool Circuit::evaluate(std::span<const bool> inputs) const {
    if (inputs.size() != inputCount_) {
        throw std::invalid_argument(
            "Incorrect number of circuit inputs."
        );
    }

    if (!output_.has_value()) {
        throw std::logic_error(
            "Circuit output has not been configured."
        );
    }

    std::vector<bool> gateOutputs;
    gateOutputs.reserve(gates_.size());

    // Evaluate gates in the order they were added.
    for (const auto& node : gates_) {
        std::array<bool, 2> gateInputs{};

        for (std::size_t i = 0; i < node.sources.size(); ++i) {
            const auto& source = node.sources[i];

            if (source.kind == SignalSource::Kind::Input) {
                gateInputs[i] = inputs[source.index];
            }
            else {
                gateInputs[i] = gateOutputs[source.index];
            }
        }

        const std::span<const bool> values{
            gateInputs.data(),
            node.sources.size()
        };

        gateOutputs.push_back(node.gate.evaluate(values));
    }

    const auto& output = output_.value();

    if (output.kind == SignalSource::Kind::Input) {
        return inputs[output.index];
    }

    return gateOutputs[output.index];
}
