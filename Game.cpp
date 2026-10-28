#include "Game.h"
#include "Circuit.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
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
    constexpr std::size_t kTruthRowsPerPage = 5;


    // Returns value of a particular input in a truth table row. A is most significant input, then B, C, etc.
    [[nodiscard]] bool inputValueForRow(
        std::size_t row,
        std::size_t inputIndex,
        std::size_t inputCount
    ) {
        const std::size_t bitPosition =
            inputCount - inputIndex - 1;

        return (row & (std::size_t{ 1 } << bitPosition)) != 0;
    }

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
        if (source.size() == 1 &&
            source[0] >= 'A' &&
            static_cast<std::size_t>(source[0] - 'A') <
            level.inputCount) {

            const std::size_t index =
                static_cast<std::size_t>(source[0] - 'A');

            // Preserve layout of existing puzzles.
            if (level.inputCount == 2) {
                return {
                    430.0f,
                    index == 0 ? 280.0f : 420.0f
                };
            }

            // Distribute additional inputs vertically.
            constexpr float top = 205.0f;
            constexpr float bottom = 445.0f;

            const float fraction =
                static_cast<float>(index) /
                static_cast<float>(level.inputCount - 1);

            return {
                430.0f,
                top + (bottom - top) * fraction
            };
        }

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

            handleMouseClick(
                event.button.x,
                event.button.y,
                removeGate
            );
        }
        else if (event.type == SDL_EVENT_KEY_DOWN) {
            const Level& level = levelManager_.currentLevel();

            const std::size_t pageCount =
                (level.expectedOutputs.size() +
                    kTruthRowsPerPage - 1) /
                kTruthRowsPerPage;

            if (event.key.scancode == SDL_SCANCODE_PAGEUP) {
                if (truthTablePage_ > 0) {
                    --truthTablePage_;
                }
            }
            else if (
                event.key.scancode == SDL_SCANCODE_PAGEDOWN
                ) {
                if (truthTablePage_ + 1 < pageCount) {
                    ++truthTablePage_;
                }
            }
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
        truthTablePage_ = 0;


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
    Circuit circuit{ level.inputCount };

    // Maps level socket IDs to Circuit gate IDs.
    std::unordered_map<std::string, Circuit::GateId> gateIds;

    for (const auto& socket : level.sockets) {
        std::vector<Circuit::SignalSource> sources;

        for (const auto& source : socket.sources) {
            if (source.size() == 1 &&
                source[0] >= 'A' &&
                static_cast<std::size_t>(source[0] - 'A') <
                level.inputCount) {

                const std::size_t inputIndex =
                    static_cast<std::size_t>(source[0] - 'A');

                sources.push_back(
                    Circuit::SignalSource::input(inputIndex)
                );
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

    // Span based circuit needs contiguous storage. Bool array provides it.
    auto inputs = std::make_unique<bool[]>(level.inputCount);

    for (std::size_t row = 0; row < level.expectedOutputs.size(); ++row) {

        for (std::size_t input = 0; input < level.inputCount; ++input) {

            inputs[input] = inputValueForRow(row, input, level.inputCount);
        }

        const bool actual = circuit.evaluate(
            std::span<const bool>{ inputs.get(), level.inputCount }
        );

        if (actual != level.expectedOutputs[row]) {
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
            statusMessage_ =
                "710 VERIFIED - ALL " + std::to_string(level.expectedOutputs.size()) + " TESTS PASSED";
        }
    }
    else {
        verificationStatus_ = VerificationStatus::Failed;

        statusMessage_ =
            "ERROR 409: " + std::to_string(failedTests) + " OF " +
            std::to_string(level.expectedOutputs.size()) + " TESTS FAILED";
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

    // Build the heading.
    std::string heading;

    for (std::size_t i = 0; i < level.inputCount; ++i) {
        heading += static_cast<char>('A' + i);
        heading += "  ";
    }

    heading += "| Y";

    drawText(renderer, 60, 305, heading);

    // Determine which truth-table rows are visible.
    const std::size_t firstRow = truthTablePage_ * kTruthRowsPerPage;

    const std::size_t lastRow = std::min(
        firstRow + kTruthRowsPerPage,
        level.expectedOutputs.size()
    );

    for (std::size_t row = firstRow; row < lastRow; ++row) {

        std::string rowText;

        for (std::size_t input = 0; input < level.inputCount; ++input) {

            const bool value = inputValueForRow(
                row,
                input,
                level.inputCount
            );

            rowText += value ? '1' : '0';
            rowText += "  ";
        }

        rowText += "| ";
        rowText += level.expectedOutputs[row] ? '1' : '0';

        drawText(
            renderer,
            60.0f,
            330.0f +
                static_cast<float>(row - firstRow) * 21.0f,
            rowText
        );
    }

    const std::size_t pageCount =
        (level.expectedOutputs.size() +
            kTruthRowsPerPage - 1) /
        kTruthRowsPerPage;

    drawText(
        renderer,
        60,
        440,
        "PAGE " +
        std::to_string(truthTablePage_ + 1) +
        "/" +
        std::to_string(pageCount) +
        "  PGUP/PGDN"
    );

    
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
    for (std::size_t i = 0; i < level.inputCount; ++i) {
        const std::string name(1, static_cast<char>('A' + i));

        const SDL_FPoint position = sourcePosition(level, name);

        drawText(renderer, 390.0f, position.y - 5.0f, name);

        // Mark the terminal where the signal begins.
        const SDL_FRect terminal{
            position.x - 3.0f,
            position.y - 3.0f,
            6.0f,
            6.0f
        };

        checkSDL(SDL_RenderFillRect(renderer, &terminal));
    }
    // ------------------------------

 
    // Draw circuit connections
    // ------------------------------
    checkSDL(SDL_SetRenderDrawColor(
        renderer, 75, 140, 95, 255
    ));

    for (const auto& socket : level.sockets) {
        for (std::size_t i = 0; i < socket.sources.size(); ++i) {
            const SDL_FPoint from = sourcePosition(level, socket.sources[i]);

            const SDL_FPoint to = inputPinPosition(socket, i);

            // Keep final vertical section just before destination socket.
            // Separate two-input connections into slightly different lanes to improve readability.
            const float pinOffset = socket.sources.size() == 2 ? (i == 0 ? 32.0f : 45.0f) : 18.0f;

            const float proposedBendX = to.x - pinOffset;

            // Allow for sockets positioned close together.
            const float bendX = proposedBendX > from.x ? proposedBendX : (from.x + to.x) * 0.5f;

            // Segment 1: horizontal from source.
            checkSDL(SDL_RenderLine(
                renderer,
                from.x, from.y,
                bendX, from.y
            ));

            // Segment 2: vertical towards input pin.
            checkSDL(SDL_RenderLine(
                renderer,
                bendX, from.y,
                bendX, to.y
            ));

            // Segment 3: horizontal into socket.
            checkSDL(SDL_RenderLine(
                renderer,
                bendX, to.y,
                to.x, to.y
            ));

            // Mark actual destination pin.
            const SDL_FRect pinMarker{
                to.x - 2.0f,
                to.y - 2.0f,
                4.0f,
                4.0f
            };

            checkSDL(SDL_RenderFillRect(renderer, &pinMarker));
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
            drawText(renderer, socket.x + 25.0f, socket.y + 24.0f, "EMPTY");
        }
    }

    // Mark final output terminal matching connection markers on external inputs.
    const SDL_FRect outputTerminal{
        902.0f,
        outputPosition.y - 3.0f,
        6.0f,
        6.0f
    };

    checkSDL(SDL_RenderFillRect(renderer, &outputTerminal));

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
