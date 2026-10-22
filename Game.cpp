#include "Game.h"
#include "Circuit.h"

#include <array>
#include <stdexcept>
#include <string>

// Helpers used only within Game.cpp.
namespace {

    // Coordinates used by rendering and input.
    constexpr SDL_FRect kGateSocket{ 615, 295, 160, 85 };

    constexpr SDL_FRect kAndButton{ 990, 235, 215, 50 };
    constexpr SDL_FRect kOrButton{ 990, 310, 215, 50 };
    constexpr SDL_FRect kNotButton{ 990, 385, 215, 50 };

    constexpr SDL_FRect kRunButton{ 615, 485, 160, 45 };
    constexpr SDL_FRect kResetButton{ 795, 485, 110, 45 };

    [[nodiscard]] bool containsPoint(
        const SDL_FRect& rect,
        float x,
        float y
    ) {
        return x >= rect.x &&
            x < rect.x + rect.w &&
            y >= rect.y &&
            y < rect.y + rect.h;
    }

    [[nodiscard]] std::string gateName(GateType type) {
        switch (type) {
        case GateType::And:
            return "AND";

        case GateType::Or:
            return "OR";

        case GateType::Not:
            return "NOT";
        }

        throw std::logic_error("Unknown gate type.");
    }

    void checkSDL(bool success) {
        if (!success) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    void drawText(
        SDL_Renderer* renderer,
        float x,
        float y,
        const std::string& text
    ) {
        checkSDL(
            SDL_RenderDebugText(
                renderer,
                x,
                y,
                text.c_str()
            )
        );
    }

}


// SDLContext implementation

SDLContext::SDLContext() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(
            std::string{ "SDL initialisation failed: " } +
            SDL_GetError()
        );
    }
}

SDLContext::~SDLContext() {
    SDL_Quit();
}

// Game implementation

Game::Game() {
    constexpr int width = 1280;
    constexpr int height = 720;

    const std::string title{ "710" };

    window_.reset(
        SDL_CreateWindow(
            title.c_str(),
            width,
            height,
            SDL_WINDOW_RESIZABLE
        )
    );

    if (!window_) {
        throw std::runtime_error(
            std::string{ "Window creation failed: " } +
            SDL_GetError()
        );
    }

    renderer_.reset(
        SDL_CreateRenderer(window_.get(), nullptr)
    );

    if (!renderer_) {
        throw std::runtime_error(
            std::string{ "Renderer creation failed: " } +
            SDL_GetError()
        );
    }

    // Maintain UI aspect ratio regardless of window size.
    if (!SDL_SetRenderLogicalPresentation(
        renderer_.get(),
        1280,
        720,
        SDL_LOGICAL_PRESENTATION_LETTERBOX
    )) {
        throw std::runtime_error(
            std::string{ "Logical presentation failed: " } +
            SDL_GetError()
        );
    }

}

void Game::run() {
    while (running_) {
        handleEvents();

        if (!running_) {
            break;
        }

        render();

        SDL_Delay(16);
    }
}

void Game::handleEvents() {
    SDL_Event event{};

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running_ = false;
        }
        else if (
            event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT
            ) {
            // Convert actual window coordinates to 1280 x 720 coordinate system.
            checkSDL(SDL_ConvertEventToRenderCoordinates(
                renderer_.get(),
                &event
            ));

            handleMouseClick(event.button.x, event.button.y);
        }
    }
}


void Game::handleMouseClick(float x, float y) {
    // Select gate from inventory.
    if (containsPoint(kAndButton, x, y)) {
        selectedGate_ = GateType::And;
        statusMessage_ = "AND SELECTED - CLICK THE SOCKET";
        return;
    }

    if (containsPoint(kOrButton, x, y)) {
        selectedGate_ = GateType::Or;
        statusMessage_ = "OR SELECTED - CLICK THE SOCKET";
        return;
    }

    if (containsPoint(kNotButton, x, y)) {
        selectedGate_ = GateType::Not;
        statusMessage_ = "NOT SELECTED - REQUIRES ONE INPUT";
        return;
    }

    // Place selected gate into circuit socket.
    if (containsPoint(kGateSocket, x, y)) {
        if (!selectedGate_.has_value()) {
            statusMessage_ = "SELECT A GATE FIRST";
            return;
        }

        const LogicGate gate{ *selectedGate_ };

        if (gate.inputCount() != 2) {
            statusMessage_ =
                "ERROR 422: SOCKET REQUIRES TWO INPUTS";
            return;
        }

        placedGate_ = selectedGate_;

        // The circuit has changed so previous verification result is no longer valid.
        verificationStatus_ = VerificationStatus::Untested;

        statusMessage_ =
            gateName(*placedGate_) + " PLACED - RUN TESTS";

        return;
    }

    // Run circuit tests.
    if (containsPoint(kRunButton, x, y)) {
        runPuzzleTests();
        return;
    }

    // Clear circuit and selection.
    if (containsPoint(kResetButton, x, y)) {
        selectedGate_.reset();
        placedGate_.reset();

        verificationStatus_ = VerificationStatus::Untested;
        statusMessage_ = "CIRCUIT RESET";

        return;
    }
}


void Game::runPuzzleTests() {
    if (!placedGate_.has_value()) {
        verificationStatus_ = VerificationStatus::Failed;
        statusMessage_ = "ERROR 404: GATE SOCKET IS EMPTY";
        return;
    }

    // Build a two-input circuit using the player's gate.
    Circuit circuit{ 2 };

    const auto gateId = circuit.addGate(
        *placedGate_,
        {
            Circuit::SignalSource::input(0),
            Circuit::SignalSource::input(1)
        }
    );

    circuit.setOutput(
        Circuit::SignalSource::gate(gateId)
    );

    // All possible combinations of A and B.
    const std::array testCases{
        std::array{false, false},
        std::array{false, true},
        std::array{true, false},
        std::array{true, true}
    };

    // Work Order 01 requires AND behaviour.
    const std::array expectedOutputs{
        false, false, false, true
    };

    std::size_t failedTests = 0;

    for (std::size_t i = 0; i < testCases.size(); ++i) {
        const bool actual = circuit.evaluate(testCases[i]);

        if (actual != expectedOutputs[i]) {
            ++failedTests;
        }
    }

    if (failedTests == 0) {
        verificationStatus_ = VerificationStatus::Passed;
        statusMessage_ =
            "710 VERIFIED - ALL FOUR TESTS PASSED";
    }
    else {
        verificationStatus_ = VerificationStatus::Failed;

        statusMessage_ =
            "ERROR 409: " +
            std::to_string(failedTests) +
            " OF 4 TESTS FAILED";
    }
}



void Game::drawPuzzlePreview() {
    SDL_Renderer* renderer = renderer_.get();

    // Terminal heading
    checkSDL(SDL_SetRenderDrawColor(
        renderer, 115, 220, 145, 255
    ));

    drawText(renderer, 40, 40, "710 // MAINTENANCE TERMINAL");
    drawText(renderer, 40, 65, "WORK ORDER 01 - AUXILIARY POWER");

    // Define four interface panels.
    const SDL_FRect objectivePanel{ 40, 105, 295, 470 };
    const SDL_FRect circuitPanel{ 350, 105, 595, 470 };
    const SDL_FRect inventoryPanel{ 960, 105, 280, 470 };
    const SDL_FRect logPanel{ 40, 590, 1200, 105 };

    // Draw panel outlines.
    checkSDL(SDL_SetRenderDrawColor(
        renderer, 65, 115, 80, 255
    ));

    checkSDL(SDL_RenderRect(renderer, &objectivePanel));
    checkSDL(SDL_RenderRect(renderer, &circuitPanel));
    checkSDL(SDL_RenderRect(renderer, &inventoryPanel));
    checkSDL(SDL_RenderRect(renderer, &logPanel));

    // Brighter text colour.
    checkSDL(SDL_SetRenderDrawColor(
        renderer, 115, 220, 145, 255
    ));


    // Objective panel
    // ------------------------------
    drawText(renderer, 60, 130, "OBJECTIVE");
    drawText(renderer, 60, 175, "Restore auxiliary power.");
    drawText(renderer, 60, 200, "Both inputs must be active.");

    drawText(renderer, 60, 270, "TARGET TRUTH TABLE");

    drawText(renderer, 60, 305, "A    B    Y");
    drawText(renderer, 60, 330, "0    0    0");
    drawText(renderer, 60, 355, "0    1    0");
    drawText(renderer, 60, 380, "1    0    0");
    drawText(renderer, 60, 405, "1    1    1");
    // ------------------------------


    // Circuit board
    // ------------------------------

    drawText(renderer, 375, 130, "CIRCUIT BOARD");

    // Show the current verification status.
    switch (verificationStatus_) {
    case VerificationStatus::Untested:
        drawText(renderer, 375, 185, "STATUS: NOT VERIFIED");
        break;

    case VerificationStatus::Passed:
        drawText(renderer, 375, 185, "STATUS: 710 VERIFIED");
        break;

    case VerificationStatus::Failed:
        drawText(renderer, 375, 185, "STATUS: VERIFICATION FAILED");
        break;
    }

    // Draw gate socket.
    checkSDL(SDL_RenderRect(renderer, &kGateSocket));

    // Input A to socket
    checkSDL(SDL_RenderLine(
        renderer, 435, 298, 615, 320
    ));

    // Input B to socket
    checkSDL(SDL_RenderLine(
        renderer, 435, 418, 615, 350
    ));

    // Socket to output Y
    checkSDL(SDL_RenderLine(
        renderer, 775, 338, 875, 338
    ));

    drawText(renderer, 398, 291, "A");
    drawText(renderer, 398, 411, "B");
    drawText(renderer, 885, 332, "Y");

    // Display gate that has been placed.
    if (placedGate_.has_value()) {
        drawText(renderer, 680, 333, gateName(*placedGate_));
    }
    else {
        drawText(renderer, 680, 333, "EMPTY");
    }

    // Draw testing controls.
    checkSDL(SDL_RenderRect(renderer, &kRunButton));
    checkSDL(SDL_RenderRect(renderer, &kResetButton));

    drawText(renderer, 640, 504, "RUN TESTS");
    drawText(renderer, 817, 504, "RESET");
    // ------------------------------



    // Gate inventory
    // ------------------------------
    drawText(renderer, 985, 130, "AVAILABLE GATES");

    // Draw inventory buttons using shared rectangles.
    checkSDL(SDL_RenderRect(renderer, &kAndButton));
    checkSDL(SDL_RenderRect(renderer, &kOrButton));
    checkSDL(SDL_RenderRect(renderer, &kNotButton));

    drawText(renderer, 1010, 255, "AND");
    drawText(renderer, 1010, 330, "OR");
    drawText(renderer, 1010, 405, "NOT");

    // Display player's current selection.
    if (selectedGate_.has_value()) {
        drawText(
            renderer,
            985,
            480,
            "SELECTED: " + gateName(*selectedGate_)
        );
    }
    else {
        drawText(renderer, 985, 480, "SELECTED: NONE");
    }
    // ------------------------------


   
    // System log
    // ------------------------------
    drawText(renderer, 60, 610, "SYSTEM LOG");
    drawText(renderer, 60, 640, "> WORK ORDER 01 LOADED");
    drawText(renderer, 60, 660, "> " + statusMessage_);
    // ------------------------------


}


void Game::render() {
    SDL_Renderer* renderer = renderer_.get();

    // Set background colour.
    checkSDL(SDL_SetRenderDrawColor(
        renderer, 8, 18, 12, 255
    ));

    // Clear previous frame.
    checkSDL(SDL_RenderClear(renderer));

    // Draw puzzle interface.
    drawPuzzlePreview();

    // Present finished frame.
    checkSDL(SDL_RenderPresent(renderer));
}
