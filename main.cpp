#include "Game.h"
#include "LogicGate.h"
#include "Circuit.h"

#include <SDL3/SDL_main.h>

#include <string>
#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>

// Temporary development tests.
// Will move to dedicated file later.
static void testLogicGates() {
    const LogicGate andGate{ GateType::And };
    const LogicGate orGate{ GateType::Or };
    const LogicGate notGate{ GateType::Not };

    const std::array testCases{
        std::array{false, false},
        std::array{false, true},
        std::array{true, false},
        std::array{true, true}
    };

    const std::array expectedAnd{ false, false, false, true };
    const std::array expectedOr{ false, true, true, true };
    const std::array expectedNot{ true, true, false, false };

    for (std::size_t i = 0; i < testCases.size(); ++i) {
        const auto& inputs = testCases[i];
        const std::array notInput{ inputs[0] };

        if (andGate.evaluate(inputs) != expectedAnd[i] ||
            orGate.evaluate(inputs) != expectedOr[i] ||
            notGate.evaluate(notInput) != expectedNot[i]) {
            throw std::runtime_error("Logic gate test failed.");
        }
    }

    std::cout << "All 12 logic gate checks passed.\n";
}


static void testCircuit() {
    Circuit circuit{ 2 };

    const auto A = Circuit::SignalSource::input(0);
    const auto B = Circuit::SignalSource::input(1);

    // Gate 0: A OR B
    const auto orGate = circuit.addGate(GateType::Or, {
        A, B
    });

    // Gate 1: A AND B
    const auto andGate = circuit.addGate(GateType::And, {
        A, B
    });

    // Gate 2: NOT (A AND B)
    const auto notGate = circuit.addGate(GateType::Not, {
        Circuit::SignalSource::gate(andGate)
    });

    // Gate 3: Combine the two branches.
    const auto finalGate = circuit.addGate(GateType::And, {
        Circuit::SignalSource::gate(orGate),
        Circuit::SignalSource::gate(notGate)
    });

    circuit.setOutput(
        Circuit::SignalSource::gate(finalGate)
    );

    // All possible input combinations.
    const std::array testCases{
        std::array{false, false},
        std::array{false, true},
        std::array{true, false},
        std::array{true, true}
    };

    // Correct XOR outputs.
    const std::array expected{
        false, true, true, false
    };

    for (std::size_t i = 0; i < testCases.size(); ++i) {
        const bool result = circuit.evaluate(testCases[i]);

        if (result != expected[i]) {
            throw std::runtime_error(
                "XOR circuit failed test case " +
                std::to_string(i)
            );
        }
    }

    std::cout << "XOR circuit passed all 4 tests.\n";
}


int main(int, char* []) {
    try {
        testLogicGates();
		testCircuit();

        Game game;
        game.run();

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
