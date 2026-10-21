
#include "Game.h"
#include "LogicGate.h"

#include <SDL3/SDL_main.h>

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>

// Temporary development tests.
// Will move to dedicated file later.
void testLogicGates() {
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

int main(int, char* []) {
    try {
        testLogicGates();

        Game game;
        game.run();

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
