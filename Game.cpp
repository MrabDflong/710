#include "Game.h"
#include "Circuit.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Helpers used only within Game.cpp.
namespace {

    // Coordinates used by rendering and input.
    constexpr SDL_FRect kRunButton  { 615, 485, 160, 45 };
    constexpr SDL_FRect kResetButton{ 795, 485, 110, 45 };
    constexpr SDL_FRect kNextButton { 615, 535, 290, 34 };

    // Input combinations in truth table order.
    constexpr std::array kInputCases{
        std::array{false, false},
        std::array{false, true},
        std::array{true, false},
        std::array{true, true}
    };

    // Find level files relative to exe.
    [[nodiscard]] std::filesystem::path getLevelDirectory() {
        const char* basePath = SDL_GetBasePath();

        if (basePath == nullptr) {
            throw std::runtime_error(
                std::string{ "Could not locate executable: " } +
                SDL_GetError()
            );
        }

        return std::filesystem::path{ basePath }
            / "assets"
            / "levels";
    }

    [[nodiscard]] bool containsPoint(const SDL_FRect& rect, float x, float y) {
        return x >= rect.x &&
            x < rect.x + rect.w &&
            y >= rect.y &&
            y < rect.y + rect.h;
    }


    [[nodiscard]] SDL_FRect socketRect(
        const SocketDefinition& socket
    ) {
        return {socket.x, socket.y, 100.0f, 56.0f};
    }

    [[nodiscard]] SDL_FRect gateButtonRect(std::size_t index) {
        constexpr float top = 205.0f;
        constexpr float spacing = 48.0f;

        return {
            990.0f,
            top + static_cast<float>(index) * spacing,
            215.0f,
            41.0f
        };
    }

    [[nodiscard]] SDL_FPoint sourcePosition(
        const Level& level,
        const std::string& source
    ) {
        // Fixed locations for external inputs.
        if (source == "A") {
            return { 430.0f, 280.0f };
        }

        if (source == "B") {
            return { 430.0f, 420.0f };
        }

        // Otherwise, the signal comes from another socket.
        const auto it = std::find_if(
            level.sockets.begin(),
            level.sockets.end(),
            [&](const SocketDefinition& socket) {
                return socket.id == source;
            }
        );

        if (it == level.sockets.end()) {
            throw std::logic_error("Unknown connection source: " + source);
        }

        return {
            it->x + 100.0f,
            it->y + 28.0f
        };
    }

    [[nodiscard]] SDL_FPoint inputPinPosition(
        const SocketDefinition& socket,
        std::size_t pin
    ) {
        // A single-input socket has one centred pin.
        if (socket.sources.size() == 1) {
            return { socket.x, socket.y + 28.0f };
        }

        // Two-input sockets have separate upper/lower pins.
        return {socket.x, socket.y + (pin == 0 ? 18.0f : 38.0f)};
    }


    [[nodiscard]] std::string gateName(GateType type) {
        switch (type) {
        case GateType::And:
            return "AND";

        case GateType::Or:
            return "OR";

        case GateType::Not:
            return "NOT";

        case GateType::Wire:
            return "WIRE";

        case GateType::Nand:
            return "NAND";

        case GateType::Xor:
            return "XOR";
        }

        throw std::logic_error("Unknown gate type.");
    }

    void checkSDL(bool success) {
        if (!success) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    void drawText(SDL_Renderer* renderer, float x, float y, const std::string& text) {
        checkSDL(SDL_RenderDebugText(renderer, x, y, text.c_str()));
    }

}


// SDLContext implementation

SDLContext::SDLContext() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(
            std::string{ "SDL initialisation failed: " } + SDL_GetError());
    }
}

SDLContext::~SDLContext() {
    SDL_Quit();
}

// Game implementation

Game::Game() : levelManager_{getLevelDirectory() } {
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
        throw std::runtime_error(std::string{ "Window creation failed: " } + SDL_GetError());
    }

    renderer_.reset(SDL_CreateRenderer(window_.get(), nullptr));

    if (!renderer_) {
        throw std::runtime_error(std::string{ "Renderer creation failed: " } + SDL_GetError()
        );
    }

    // Maintain UI aspect ratio regardless of window size.
    if (!SDL_SetRenderLogicalPresentation(
        renderer_.get(), 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX)
    ) {
        throw std::runtime_error(
            std::string{ "Logical presentation failed: " } + SDL_GetError()
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
            (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)) {
            checkSDL(SDL_ConvertEventToRenderCoordinates(renderer_.get(), &event));

            const bool removeGate = event.button.button == SDL_BUTTON_RIGHT;

            handleMouseClick(event.button.x, event.button.y, removeGate);
        }
    }
}



void Game::handleMouseClick( float x, float y, bool removeGate) {
    const Level& level = levelManager_.currentLevel();


    // Right click removes a gate.
    // ------------------------------
    if (removeGate) {
        for (const auto& socket : level.sockets) {
            if (!containsPoint(socketRect(socket), x, y)) {
                continue;
            }

            if (placedGates_.erase(socket.id) > 0) {
                verificationStatus_ = VerificationStatus::Untested;

                statusMessage_ = "SOCKET " + socket.id + " CLEARED";
            }

            return;
        }

        return;
    }
    // ------------------------------


    // Select an available gate.
    // ------------------------------
    for (std::size_t i = 0; i < level.availableGates.size(); ++i) {
        const GateType type = level.availableGates[i];

        if (containsPoint(gateButtonRect(i), x, y)) {
            selectedGate_ = type;

            statusMessage_ = gateName(type) + " SELECTED - CLICK A SOCKET";

            return;
        }
    }
    // ------------------------------

    
    // Place or replace a gate.
    // ------------------------------
    for (const auto& socket : level.sockets) {
        if (!containsPoint(socketRect(socket), x, y)) {
            continue;
        }

        if (!selectedGate_.has_value()) {
            statusMessage_ = "SELECT A GATE FIRST";
            return;
        }

        const LogicGate gate{ *selectedGate_ };

        // AND/OR require two pins. NOT requires one.
        if (gate.inputCount() != socket.sources.size()) {
            statusMessage_ = "ERROR 422: INCOMPATIBLE SOCKET";
            return;
        }

        placedGates_.insert_or_assign( socket.id, *selectedGate_);

        verificationStatus_ = VerificationStatus::Untested;

        statusMessage_ = gateName(*selectedGate_) + " PLACED IN " + socket.id;

        return;
    }
    // ------------------------------

    
    // Run circuit verification.
    // ------------------------------
    if (containsPoint(kRunButton, x, y)) {
        runPuzzleTests();
        return;
    }
    // ------------------------------


    // Reset all placed gates.
    // ------------------------------
    if (containsPoint(kResetButton, x, y)) {
        selectedGate_.reset();
        placedGates_.clear();

        verificationStatus_ = VerificationStatus::Untested;

        statusMessage_ = "CIRCUIT RESET";
        return;
    }
    // ------------------------------


    // Progress to next work order.
    // ------------------------------
    if (containsPoint(kNextButton, x, y) &&
        verificationStatus_ == VerificationStatus::Passed &&
        levelManager_.hasNextLevel()) {

        levelManager_.nextLevel();

        selectedGate_.reset();
        placedGates_.clear();

        verificationStatus_ = VerificationStatus::Untested;

        const Level& nextLevel = levelManager_.currentLevel();

        if (!nextLevel.arrivalMessage.empty()) {
            statusMessage_ = nextLevel.arrivalMessage;
        }
        else {
            statusMessage_ = "NEW WORK ORDER LOADED";
        }

        return;
    }
    // ------------------------------
}




void Game::runPuzzleTests() {
    const Level& level = levelManager_.currentLevel();


    // Check for empty sockets
    // ------------------------------
    for (const auto& socket : level.sockets) {
        if (!placedGates_.contains(socket.id)) {
            verificationStatus_ = VerificationStatus::Failed;

            statusMessage_ ="ERROR 404: SOCKET " + socket.id + " IS EMPTY";

            return;
        }
    }
    // ------------------------------


    // Construct the circuit
    // ------------------------------
    Circuit circuit{ 2 };

    // Maps level socket IDs to Circuit gate IDs.
    std::unordered_map<std::string, Circuit::GateId> gateIds;

    for (const auto& socket : level.sockets) {
        std::vector<Circuit::SignalSource> sources;

        for (const auto& source : socket.sources) {
            if (source == "A") {
                sources.push_back(Circuit::SignalSource::input(0));
            }
            else if (source == "B") {
                sources.push_back(Circuit::SignalSource::input(1));
            }
            else {
                // Source refers to a previous socket.
                sources.push_back(
                    Circuit::SignalSource::gate(
                        gateIds.at(source)
                    )
                );
            }
        }

        const auto gateId = circuit.addGate(
            placedGates_.at(socket.id),
            std::move(sources)
        );

        gateIds.emplace(socket.id, gateId);
    }

    // Connect specified final gate to output Y.
    circuit.setOutput(Circuit::SignalSource::gate(
        gateIds.at(level.outputSocket))
    );
    // ------------------------------


    // Verify against truth table
    // ------------------------------
    std::size_t failedTests = 0;

    for (std::size_t i = 0; i < kInputCases.size(); ++i) {
        const bool actual = circuit.evaluate(kInputCases[i]);

        if (actual != level.expectedOutputs[i]) {
            ++failedTests;
        }
    }
    // ------------------------------


    // Report result
    // ------------------------------
    if (failedTests == 0) {
        verificationStatus_ = VerificationStatus::Passed;

        // Count every placed component except WIRE.
        const auto gateCount = static_cast<std::size_t>(
            std::count_if(
                placedGates_.begin(),
                placedGates_.end(),
                [](const auto& placement) {
                    return placement.second != GateType::Wire;
                }
            )
        );

        // Optional engineering merit is awarded only when circuit works correctly.
        if (level.optimalGateCount.has_value() &&
            gateCount <= *level.optimalGateCount) {

            engineeringMerits_.insert(level.id);
        }

        // Report component certification if applicable.
        if (level.certifies.has_value()) {
            statusMessage_ =
                "710 VERIFIED - " +
                gateName(*level.certifies) +
                " CERTIFIED";
        }
        else {
            statusMessage_ = "710 VERIFIED - ALL FOUR TESTS PASSED";
        }
    }
    else {
        verificationStatus_ = VerificationStatus::Failed;

        statusMessage_ =
            "ERROR 409: " + std::to_string(failedTests) + " OF 4 TESTS FAILED";
    }
    // ------------------------------
}
    





void Game::drawPuzzlePreview() {
    SDL_Renderer* renderer = renderer_.get();
    const Level& level = levelManager_.currentLevel();


    // Terminal heading
    // ------------------------------
    checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));

    drawText(renderer, 40, 40, "710 // MAINTENANCE TERMINAL");

    drawText(
        renderer,
        40,
        65,
        "WORK ORDER " + level.id + " - " + level.title
    );
    // ------------------------------


    // Main interface panels
    // ------------------------------
    const SDL_FRect objectivePanel  { 40, 105, 295, 470 };
    const SDL_FRect circuitPanel    { 350, 105, 595, 470 };
    const SDL_FRect inventoryPanel  { 960, 105, 280, 470 };
    const SDL_FRect logPanel        { 40, 590, 1200, 105 };

    checkSDL(SDL_SetRenderDrawColor(renderer, 65, 115, 80, 255));

    checkSDL(SDL_RenderRect(renderer, &objectivePanel));
    checkSDL(SDL_RenderRect(renderer, &circuitPanel));
    checkSDL(SDL_RenderRect(renderer, &inventoryPanel));
    checkSDL(SDL_RenderRect(renderer, &logPanel));

    checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));
    // ------------------------------


    // Objective panel
    // ------------------------------
    drawText(renderer, 60, 130, "OBJECTIVE");
    drawText(renderer, 60, 175, level.objective);
    drawText(renderer, 60, 200, level.hint);

    drawText(renderer, 60, 270, "TARGET TRUTH TABLE");
    drawText(renderer, 60, 305, "A    B    Y");

    for (std::size_t i = 0; i < kInputCases.size(); ++i) {
        const auto& inputs = kInputCases[i];

        const std::string row =
            std::to_string(static_cast<int>(inputs[0])) +
            "    " +
            std::to_string(static_cast<int>(inputs[1])) +
            "    " +
            std::to_string(
                static_cast<int>(level.expectedOutputs[i])
            );

        drawText(
            renderer,
            60,
            330.0f + static_cast<float>(i) * 25.0f,
            row
        );
    }

    
    // Gate usage and Engineering Merit
    // ------------------------------
    const auto gateCount = static_cast<std::size_t>(
        std::count_if(
            placedGates_.begin(),
            placedGates_.end(),
            [](const auto& placement) {
                return placement.second != GateType::Wire;
            }
        )
    );

    drawText(
        renderer,
        60,
        465,
        "LOGIC GATES: " + std::to_string(gateCount)
    );

    if (level.optimalGateCount.has_value()) {
        drawText(
            renderer,
            60,
            490,
            "MERIT TARGET: " +
            std::to_string(*level.optimalGateCount)
        );

        if (engineeringMerits_.contains(level.id)) {
            drawText(
                renderer,
                60,
                515,
                "ENGINEERING MERIT: EARNED"
            );
        }
        else if (
            verificationStatus_ == VerificationStatus::Passed
            ) {
            drawText(
                renderer,
                60,
                515,
                "MERIT: NOT EARNED"
            );
        }
    }
    // ------------------------------


    // Circuit board heading
    // ------------------------------
    drawText(renderer, 375, 130, "CIRCUIT BOARD");

    switch (verificationStatus_) {
    case VerificationStatus::Untested:
        drawText(renderer, 375, 160, "STATUS: NOT VERIFIED");
        break;

    case VerificationStatus::Passed:
        drawText(renderer, 375, 160, "STATUS: 710 VERIFIED");
        break;

    case VerificationStatus::Failed:
        drawText(renderer, 375, 160, "STATUS: VERIFICATION FAILED");
        break;
    }

    // External input labels.
    drawText(renderer, 390, 275, "A");
    drawText(renderer, 390, 415, "B");
    // ------------------------------

 
    // Draw circuit connections
    // ------------------------------
    checkSDL(SDL_SetRenderDrawColor(renderer, 75, 140, 95, 255));

    for (const auto& socket : level.sockets) {
        for (std::size_t i = 0; i < socket.sources.size(); ++i) {
            const SDL_FPoint from =
                sourcePosition(level, socket.sources[i]);

            const SDL_FPoint to =
                inputPinPosition(socket, i);

            checkSDL(SDL_RenderLine(
                renderer,
                from.x, from.y,
                to.x, to.y
            ));
        }
    }

    // Draw wire from final socket to Y.
    const SDL_FPoint outputPosition = sourcePosition(
        level, 
        level.outputSocket
    );

    checkSDL(SDL_RenderLine(
        renderer,
        outputPosition.x,
        outputPosition.y,
        905.0f,
        outputPosition.y
    ));
    // ------------------------------


    // Draw circuit sockets
    // ------------------------------
    checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));

    for (const auto& socket : level.sockets) {
        const SDL_FRect rect = socketRect(socket);

        checkSDL(SDL_RenderRect(renderer, &rect));

        // Label socket with its ID.
        drawText(
            renderer,
            socket.x + 8.0f,
            socket.y - 16.0f,
            socket.id
        );

        // Show currently placed component.
        const auto it = placedGates_.find(socket.id);

        if (it != placedGates_.end()) {
            drawText(
                renderer, 
                socket.x + 25.0f, 
                socket.y + 24.0f, 
                gateName(it->second)
            );
        }
        else {
            drawText (renderer, socket.x + 25.0f, socket.y + 24.0f, "EMPTY");
        }
    }

    // Draw testing controls.
    drawText(renderer, 913.0f, outputPosition.y - 4.0f, "Y");
    // ------------------------------


    // Testing controls
    // ------------------------------
    checkSDL(SDL_RenderRect(renderer, &kRunButton));
    checkSDL(SDL_RenderRect(renderer, &kResetButton));

    drawText(renderer, 640, 504, "RUN TESTS");
    drawText(renderer, 817, 504, "RESET");

    if (verificationStatus_ == VerificationStatus::Passed) {
        if (levelManager_.hasNextLevel()) {
            checkSDL(SDL_RenderRect(renderer, &kNextButton));
            drawText(renderer, 640, 548, "NEXT WORK ORDER");
        }
        else {
            drawText(renderer, 615, 548, "ALL WORK ORDERS COMPLETE");
        }
    }
    // ------------------------------


    // Gate inventory
    // ------------------------------
    drawText(renderer, 985, 130, "AVAILABLE GATES");

    for (std::size_t i = 0; i < level.availableGates.size(); ++i) {
        const GateType type = level.availableGates[i];
        const SDL_FRect rect = gateButtonRect(i);

        const bool selected =
            selectedGate_.has_value() && *selectedGate_ == type;

        // Give selected gate a filled background.
        if (selected) {
            checkSDL(SDL_SetRenderDrawColor(renderer, 25, 65, 40, 255));

            checkSDL(SDL_RenderFillRect(renderer, &rect));
        }

        checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));

        checkSDL(SDL_RenderRect(renderer, &rect));

        drawText(
            renderer,
            rect.x + 20.0f,
            rect.y + 15.0f,
            gateName(type)
        );
    }

    if (selectedGate_.has_value()) {
        drawText(renderer, 985, 500, "SELECTED: " + gateName(*selectedGate_));
    }
    else {
        drawText(renderer, 985, 500, "SELECTED: NONE");
    }

    drawText(renderer, 985, 535, "RIGHT CLICK TO REMOVE");
    // ------------------------------

    
    // System log
    // ------------------------------
    drawText(renderer, 60, 610, "SYSTEM LOG");

    drawText(
        renderer,
        60,
        640,
        "> WORK ORDER " + level.id + " LOADED"
    );

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
