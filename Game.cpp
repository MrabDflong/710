#include "Game.h"
#include "Circuit.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Helpers used only within Game.cpp.
namespace {

    // Coordinates used by rendering and input.
    constexpr float kSocketWidth = 78.0f;
    constexpr float kSocketHeight = 46.0f;
    
    constexpr SDL_FRect kRunButton  { 615, 485, 160, 45 };
    constexpr SDL_FRect kResetButton{ 795, 485, 110, 45 };
    constexpr SDL_FRect kNextButton { 615, 535, 290, 34 };

    constexpr std::size_t kMenuPageSize = 10;

    constexpr SDL_FRect kMenuPreviousButton{455, 555, 170, 34};

    constexpr SDL_FRect kMenuNextButton{655, 555, 170, 34};

    constexpr SDL_FRect kMenuExitButton{520, 645, 240, 38};

    constexpr SDL_FRect kSwitchSaveButton{290, 645, 200, 38};

    constexpr SDL_FRect kSaveExitButton{520, 625, 240, 38};

    constexpr SDL_FRect kConfirmDeleteButton{365, 450, 250, 48};

    constexpr SDL_FRect kCancelDeleteButton{665, 450, 250, 48};

    [[nodiscard]] SDL_FRect saveSlotRect(std::size_t slot) {
        return {
            145.0f,
            190.0f + static_cast<float>(slot) * 112.0f,
            990.0f,
            82.0f
        };
    }

    [[nodiscard]] SDL_FRect saveDeleteRect(std::size_t slot) {
        return {
            960.0f,
            209.0f + static_cast<float>(slot) * 112.0f,
            150.0f,
            43.0f
        };
    }

    [[nodiscard]] SDL_FRect menuLevelRect(std::size_t slot) {
        return {
            135.0f + static_cast<float>(slot % 2) * 515.0f,
            195.0f + static_cast<float>(slot / 2) * 67.0f,
            495.0f,
            54.0f
        };
    }
    constexpr std::size_t kTruthRowsPerPage = 5;
    constexpr SDL_FRect kBriefButton{ 60.0f, 238.0f, 245.0f, 22.0f };
    constexpr SDL_FRect kBriefCloseButton{ 920.0f, 560.0f, 140.0f, 30.0f };


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

        return std::filesystem::path{ basePath } / "assets"/ "levels";
    }

    [[nodiscard]] std::filesystem::path getSavePath(
        std::size_t slot
    ) {
        static const std::filesystem::path directory = [] {
            char* basePath = SDL_GetPrefPath("710_Project", "710");

            if (!basePath) {
                throw std::runtime_error(
                    std::string{ "Unable to find save directory: " } +
                    SDL_GetError()
                );
            }

            // SDL returns a UTF-8 path. Convert it to C++20 UTF-8 string before constructing filesystem path.
            const std::string pathText{ basePath };

            const std::u8string utf8Path{
                pathText.begin(),
                pathText.end()
            };

            const std::filesystem::path result{ utf8Path };

            SDL_free(basePath);

            return result;
            }();

        return directory / ("slot" + std::to_string(slot + 1) + ".sav");
    }

    [[nodiscard]] bool containsPoint(const SDL_FRect& rect, float x, float y) {
        return x >= rect.x &&
            x < rect.x + rect.w &&
            y >= rect.y &&
            y < rect.y + rect.h;
    }


    [[nodiscard]] SDL_FRect socketRect(const SocketDefinition& socket) {

        return {socket.x, socket.y, kSocketWidth, kSocketHeight};
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
                    index == 0 ? 230.0f : 420.0f
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

        return {it->x + kSocketWidth,it->y + kSocketHeight * 0.5f};
    }

    [[nodiscard]] SDL_FPoint inputPinPosition(const SocketDefinition& socket, std::size_t pin) {

        // A single-input socket has one centred pin.
        if (socket.sources.size() == 1) {
            return { socket.x, socket.y + kSocketHeight * 0.5f };
        }

        // Two-input sockets have separate upper/lower pins.
        return {socket.x, socket.y + (pin == 0 ? 14.0f : 32.0f)};
    }


    void checkSDL(bool success);

    void drawSignalConnections(SDL_Renderer* renderer, const Level& level, const std::string& source, std::size_t sourceIndex) {

        // Find every input pin connected to this source.
        std::vector<SDL_FPoint> destinations;

        for (const auto& socket : level.sockets) {
            for (std::size_t pin = 0; pin < socket.sources.size(); ++pin) {

                if (socket.sources[pin] == source) {
                    destinations.push_back(inputPinPosition(socket, pin));
                }
            }
        }

        if (destinations.empty()) {
            return;
        }

        const SDL_FPoint from = sourcePosition(level, source);

        // Find nearest destination column.
        float nearestX = destinations.front().x;

        for (const auto& point : destinations) {
            nearestX = std::min(nearestX, point.x);
        }

        // Give neighbouring signal sources different routing lanes within the gap between socket columns.
        const float laneFraction = 0.25f + 0.15f * static_cast<float>(sourceIndex % 4);

        const float laneX = from.x + (nearestX - from.x) * laneFraction;

        // Shared vertical trunk must span both the source and every destination.
        float minY = from.y;
        float maxY = from.y;

        for (const auto& point : destinations) {
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }

        // Horizontal segment leaving the source.
        checkSDL(SDL_RenderLine(renderer, from.x, from.y, laneX, from.y));

        // Shared vertical routing trunk.
        checkSDL(SDL_RenderLine(renderer, laneX, minY, laneX, maxY));

        // Horizontal branches entering destination pins.
        for (const auto& point : destinations) {
            checkSDL(SDL_RenderLine(renderer, laneX, point.y, point.x, point.y));
        }

        // Mark real signal junctions when source feeds more than one destination.
        if (destinations.size() > 1) {
            const auto drawJunction = [&](float y) {
                const SDL_FRect marker{
                    laneX - 2.5f,
                    y - 2.5f,
                    5.0f,
                    5.0f
                };

                checkSDL(SDL_RenderFillRect(renderer, &marker));
            };

            drawJunction(from.y);

            for (const auto& point : destinations) {
                drawJunction(point.y);
            }
        }
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

    [[nodiscard]] std::vector<std::string> wrapText(const std::string& text, std::size_t maxChars) {

        std::vector<std::string> lines;

        if (maxChars == 0) {
            return lines;
        }

        std::istringstream stream{ text };
        std::string word;
        std::string line;

        while (stream >> word) {
            // Handle individual words longer than the available width.
            while (word.size() > maxChars) {
                if (!line.empty()) {
                    lines.push_back(line);
                    line.clear();
                }

                lines.push_back(word.substr(0, maxChars));
                word.erase(0, maxChars);
            }

            if (word.empty()) {
                continue;
            }

            if (line.empty()) {
                line = word;
            }
            else if (line.size() + 1 + word.size() <= maxChars) {
                line += " " + word;
            }
            else {
                lines.push_back(line);
                line = word;
            }
        }

        if (!line.empty()) {
            lines.push_back(line);
        }

        return lines;
    }

    void drawWrappedText(
        SDL_Renderer* renderer,
        float x,
        float y,
        const std::string& text,
        std::size_t maxChars,
        std::size_t maxLines,
        float lineHeight = 18.0f
    ) {
        const auto lines = wrapText(text, maxChars);

        const std::size_t count = std::min(lines.size(), maxLines);

        for (std::size_t i = 0; i < count; ++i) {
            std::string line = lines[i];

            // Abbreviate the preview when more lines exist.
            // The complete briefing remains accessible separately.
            if (i + 1 == count &&
                lines.size() > maxLines && maxChars >= 3) {

                if (line.size() > maxChars - 3) {
                    line.resize(maxChars - 3);
                }

                line += "...";
            }

            drawText(renderer, x, y + static_cast<float>(i) * lineHeight, line);
        }
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


bool Game::loadProgress() {
    if (!activeSaveSlot_) {
        return false;
    }

    std::ifstream file{ getSavePath(*activeSaveSlot_) };

    if (!file.is_open()) {
        return false;
    }

    std::size_t completed{};

    if (!(file >> completed) || completed > levelManager_.levelCount()) {
        return false;
    }

    std::unordered_set<std::string> merits;

    std::string record;
    std::string id;

    while (file >> record) {
        if (record != "MERIT" || !(file >> id)) {
            return false;
        }

        // Only accept Merit records for completed levels.
        bool validId = false;

        for (std::size_t i = 0; i < completed; ++i) {
            if (levelManager_.levelAt(i).id == id) {
                validId = true;
                break;
            }
        }

        if (!validId) {
            return false;
        }

        merits.insert(id);
    }

    if (!file.eof()) {
        return false;
    }

    // Apply progress only after validating entire file.
    completedLevelCount_ = completed;
    engineeringMerits_ = std::move(merits);

    return true;
}

bool Game::saveProgress() const {
    if (!activeSaveSlot_) {
        return false;
    }

    std::ofstream file{getSavePath(*activeSaveSlot_), std::ios::trunc};

    if (!file.is_open()) {
        return false;
    }

    file << completedLevelCount_ << '\n';

    for (const auto& id : engineeringMerits_) {
        file << "MERIT " << id << '\n';
    }

    file.flush();

    return static_cast<bool>(file);
}


void Game::selectSaveSlot(std::size_t slot) {
    if (slot >= kSaveSlotCount) {
        return;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(getSavePath(slot), error);

    if (error) {
        saveScreenMessage_ = "ERROR: CANNOT ACCESS SAVE FILE";
        return;
    }

    activeSaveSlot_ = slot;

    completedLevelCount_ = 0;
    engineeringMerits_.clear();

    if (exists) {
        if (!loadProgress()) {
            activeSaveSlot_.reset();
            saveScreenMessage_ = "ERROR: SAVE FILE IS UNREADABLE";
            return;
        }
    }
    else {
        // Create empty save immediately.
        if (!saveProgress()) {
            activeSaveSlot_.reset();
            saveScreenMessage_ = "ERROR: CANNOT CREATE SAVE FILE";
            return;
        }
    }

    // Never transfer puzzle or log state between saves.
    selectedGate_.reset();
    placedGates_.clear();

    systemLog_.clear();
    logFirstVisible_ = 0;

    truthTablePage_ = 0;
    showBriefing_ = false;
    debugLevelOverride_ = false;

    verificationStatus_ = VerificationStatus::Untested;

    menuPage_ = 0;
    menuMessage_ = "SELECT AN AUTHORISED WORK ORDER";

    screen_ = Screen::MainMenu;

    logEvent("SAVE " + std::to_string(slot + 1) + " LOADED");
}



void Game::handleSaveSelectClick(float x, float y) {

    for (std::size_t i = 0; i < kSaveSlotCount; ++i) {
        std::error_code error;
        const bool exists = std::filesystem::exists(getSavePath(i), error);

        if (error) {
            saveScreenMessage_ = "ERROR: CANNOT ACCESS SAVE SLOT";
            return;
        }

        // Test DELETE before the larger slot rectangle
        // because the delete button sits inside it.
        if (containsPoint(saveDeleteRect(i), x, y)) {
            if (exists) {
                pendingDeleteSlot_ = i;
                screen_ = Screen::DeleteConfirmation;
            }
            return;
        }

        if (containsPoint(saveSlotRect(i), x, y)) {
            selectSaveSlot(i);
            return;
        }
    }

    if (containsPoint(kSaveExitButton, x, y)) {
        running_ = false;
    }
}

void Game::handleDeleteConfirmationClick(float x, float y) {

    if (containsPoint(kCancelDeleteButton, x, y)) {
        pendingDeleteSlot_.reset();
        screen_ = Screen::SaveSelect;
        return;
    }

    if (!containsPoint(kConfirmDeleteButton, x, y) || !pendingDeleteSlot_) {
        return;
    }

    const std::size_t slot = *pendingDeleteSlot_;

    std::error_code error;
    const bool removed = std::filesystem::remove(getSavePath(slot), error);

    pendingDeleteSlot_.reset();
    screen_ = Screen::SaveSelect;

    if (error) {
        saveScreenMessage_ = "ERROR: COULD NOT DELETE SAVE FILE";
    }
    else if (removed) {
        saveScreenMessage_ = "SAVE 0" + std::to_string(slot + 1) + " DELETED";
    }
    else {
        saveScreenMessage_ = "SAVE FILE NOT FOUND";
    }
}



void Game::logEvent(std::string message) {
    if (message.empty()) {
        return;
    }

    systemLog_.push_back(std::move(message));

    // Keep a bounded history.
    constexpr std::size_t kMaximumEntries = 100;
    constexpr std::size_t kVisibleEntries = 3;

    if (systemLog_.size() > kMaximumEntries) {
        systemLog_.erase(systemLog_.begin());
    }

    // Automatically show newest messages.
    logFirstVisible_ = systemLog_.size() > kVisibleEntries ? systemLog_.size() - kVisibleEntries : 0;
}

void Game::setStatus(std::string message) {
    statusMessage_ = std::move(message);
    logEvent(statusMessage_);
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
            event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)) {

            checkSDL(SDL_ConvertEventToRenderCoordinates(renderer_.get(), &event));

            const bool removeGate = event.button.button == SDL_BUTTON_RIGHT;

            if (screen_ == Screen::SaveSelect) {
                if (!removeGate) {
                    handleSaveSelectClick(event.button.x, event.button.y);
                }
                continue;
            }

            if (screen_ == Screen::DeleteConfirmation) {
                if (!removeGate) {
                    handleDeleteConfirmationClick(event.button.x, event.button.y);
                }
                continue;
            }

            if (screen_ == Screen::MainMenu) {
                if (!removeGate) {
                    handleMenuClick(event.button.x, event.button.y);
                }
                continue;
            }

            handleMouseClick(event.button.x, event.button.y, removeGate);
        }
        else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.scancode == SDL_SCANCODE_ESCAPE && !event.key.repeat) {

                if (screen_ == Screen::DeleteConfirmation) {
                    pendingDeleteSlot_.reset();
                    screen_ = Screen::SaveSelect;
                }
                else if (screen_ == Screen::MainMenu) {
                    activeSaveSlot_.reset();
                    screen_ = Screen::SaveSelect;
                }
                else if (screen_ == Screen::Puzzle) {
                    if (showBriefing_) {
                        showBriefing_ = false;
                    }
                    else {
                        screen_ = Screen::MainMenu;
                        menuMessage_ = "WORK ORDER CLOSED - SELECT TO RESTART";
                    }
                }
                else if (screen_ == Screen::SaveSelect) {
                    running_ = false;
                }

                continue;
            }

#ifndef NDEBUG
            // Debug-only level navigation.
            if (screen_ == Screen::Puzzle && !event.key.repeat) {
                const SDL_Scancode key = event.key.scancode;

                const std::size_t current = levelManager_.currentIndex();

                std::size_t target = current;
                bool skipRequested = false;

                // Previous work order.
                if (key == SDL_SCANCODE_F6 && current > 0) {
                    target = current - 1;
                    skipRequested = true;
                }

                // Next work order.
                else if (key == SDL_SCANCODE_F7) {
                    target = current + 1;
                    skipRequested = true;
                }

                // Ctrl + 1 to Ctrl + 9.
                else if (
                    (event.key.mod & SDL_KMOD_CTRL) != 0 &&
                    key >= SDL_SCANCODE_1 &&
                    key <= SDL_SCANCODE_9
                    ) {
                    target = static_cast<std::size_t>(key - SDL_SCANCODE_1);

                    skipRequested = true;
                }

                if (skipRequested && target < levelManager_.levelCount()) {

                    openWorkOrder(target, true);
                    setStatus("DEBUG: WORK ORDER " + levelManager_.currentLevel().id + " LOADED");

                    continue;
                }
            }
#endif
            if (screen_ != Screen::Puzzle) {
                continue;
            }

            
            // Menu keyboard events must not change puzzle UI state.
            if (screen_ == Screen::MainMenu) {
                continue;
            }

            // Browse older system-log entries using "[" & "]".
            if (!showBriefing_ && event.key.scancode == SDL_SCANCODE_LEFTBRACKET) {

                if (logFirstVisible_ > 0) {
                    --logFirstVisible_;
                }

                continue;
            }

            if (!showBriefing_ && event.key.scancode == SDL_SCANCODE_RIGHTBRACKET) {

                if (logFirstVisible_ + 3 < systemLog_.size()) {
                    ++logFirstVisible_;
                }

                continue;
            }

            if (event.key.scancode == SDL_SCANCODE_I && !event.key.repeat) {

                showBriefing_ = !showBriefing_;
                continue;
            }

            if (showBriefing_) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    showBriefing_ = false;
                }

                continue;
            }

            const Level& level = levelManager_.currentLevel();

            const std::size_t pageCount = (level.expectedOutputs.size() + kTruthRowsPerPage - 1) / kTruthRowsPerPage;

            if (event.key.scancode == SDL_SCANCODE_PAGEUP) {
                if (truthTablePage_ > 0) {
                    --truthTablePage_;
                }
            }
            else if ( event.key.scancode == SDL_SCANCODE_PAGEDOWN) {
                if (truthTablePage_ + 1 < pageCount) {
                    ++truthTablePage_;
                }
            }
        }
    }
}




void Game::handleMouseClick(float x, float y, bool removeGate) {
    const Level& level = levelManager_.currentLevel();


    // Briefing controls
    // ------------------------------
    if (showBriefing_) {
        if (!removeGate && containsPoint(kBriefCloseButton, x, y)) {
            showBriefing_ = false;
        }
        return;
    }

    if (!removeGate && containsPoint(kBriefButton, x, y)) {
        showBriefing_ = true;
        return;
    }
    // ------------------------------


    // Right-click: remove a gate
    // ------------------------------
    if (removeGate) {
        for (const auto& socket : level.sockets) {
            if (!containsPoint(socketRect(socket), x, y)) {
                continue;
            }

            if (placedGates_.erase(socket.id) > 0) {
                verificationStatus_ = VerificationStatus::Untested;

                setStatus("SOCKET " + socket.id + " CLEARED");
            }

            return;
        }

        return;
    }
    // ------------------------------


    // Select a component
    // ------------------------------
    for (std::size_t i = 0; i < level.availableGates.size(); ++i) {

        const GateType type = level.availableGates[i];

        if (containsPoint(gateButtonRect(i), x, y)) {
            selectedGate_ = type;

            setStatus(gateName(type) + " SELECTED - CLICK A SOCKET");

            return;
        }
    }
    // ------------------------------


    // Place or replace a component
    // ------------------------------
    for (const auto& socket : level.sockets) {
        if (!containsPoint(socketRect(socket), x, y)) {
            continue;
        }

        if (!selectedGate_.has_value()) {
            setStatus("SELECT A GATE FIRST");
            return;
        }

        const LogicGate gate{ *selectedGate_ };

        if (gate.inputCount() != socket.sources.size()) {
            setStatus("ERROR 422: INCOMPATIBLE SOCKET");
            return;
        }

        placedGates_.insert_or_assign(socket.id, *selectedGate_);

        verificationStatus_ = VerificationStatus::Untested;

        setStatus(gateName(*selectedGate_) + " PLACED IN " + socket.id);

        return;
    }
    // ------------------------------

    
    // Verification
    // ------------------------------
    if (containsPoint(kRunButton, x, y)) {
        runPuzzleTests();
        return;
    }
    // ------------------------------

  
    // Reset
    // ------------------------------
    if (containsPoint(kResetButton, x, y)) {
        selectedGate_.reset();
        placedGates_.clear();

        verificationStatus_ =
            VerificationStatus::Untested;

        setStatus("CIRCUIT RESET");
        return;
    }
    // ------------------------------

    
    // Next work order / return to menu
    // ------------------------------
    if (containsPoint(kNextButton, x, y) &&
        verificationStatus_ == VerificationStatus::Passed) {

        if (levelManager_.hasNextLevel()) {
            openWorkOrder(levelManager_.currentIndex() + 1, debugLevelOverride_);
        }
        else {
            screen_ = Screen::MainMenu;

            menuMessage_ = completedLevelCount_ == levelManager_.levelCount() ? "ALL WORK ORDERS VERIFIED" : "RETURNED FROM DEBUG WORK ORDER";
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

            logEvent(statusMessage_);

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

        const auto gateCount = static_cast<std::size_t>(
            std::count_if(
                placedGates_.begin(),
                placedGates_.end(),
                [](const auto& placement) {
                    return placement.second != GateType::Wire;
                }
            )
        );

        bool progressChanged = false;

        // Debug level shortcuts must never alter saved progress.
        if (!debugLevelOverride_) {
            // Engineering Merit is awarded independently from ordinary work-order completion.
            if (level.optimalGateCount.has_value() &&
                gateCount <= *level.optimalGateCount) {

                if (engineeringMerits_.insert(level.id).second) {
                    logEvent(
                        "ENGINEERING MERIT AWARDED - WORK ORDER " +
                        level.id
                    );

                    progressChanged = true;
                }
            }

            // Advance highest consecutively completed level.
            if (levelManager_.currentIndex() ==
                completedLevelCount_) {

                ++completedLevelCount_;
                progressChanged = true;

                logEvent("WORK ORDER " + level.id + " VERIFIED");
            }

            if (progressChanged && !saveProgress()) {
                logEvent("ERROR: PROGRESS COULD NOT BE SAVED");
            }
        }

        if (level.certifies.has_value()) {
            statusMessage_ = "710 VERIFIED - " + gateName(*level.certifies) + " CERTIFIED";
        }
        else {
            statusMessage_ = "710 VERIFIED - ALL " + std::to_string(level.expectedOutputs.size()) + " TESTS PASSED";
        }
    }
    else {
        verificationStatus_ = VerificationStatus::Failed;

        statusMessage_ = "ERROR 409: " + std::to_string(failedTests) + " OF " + std::to_string(level.expectedOutputs.size()) + " TESTS FAILED";
    }

    // Record either successful or failed verification.
    logEvent(statusMessage_);

    // ------------------------------
}
    

void Game::drawWorkOrderBriefing() {
    SDL_Renderer* renderer = renderer_.get();
    const Level& level = levelManager_.currentLevel();

    const SDL_FRect panel{
        170.0f, 105.0f, 940.0f, 505.0f
    };

    // Opaque background hides circuit beneath.
    checkSDL(SDL_SetRenderDrawColor(renderer, 5, 22, 12, 255));

    checkSDL(SDL_RenderFillRect(renderer, &panel));

    // Restore terminal's green drawing colour.
    checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));

    checkSDL(SDL_RenderRect(renderer, &panel));

    drawText( renderer, 190, 135, "WORK ORDER " + level.id + " - ENGINEERING BRIEF");

    drawText(renderer, 190, 175, "REQUIRED BEHAVIOUR");

    const std::string& explanation = level.briefing.empty() ? level.hint : level.briefing;

    drawWrappedText(
        renderer, 190, 200,
        explanation, 108, 3, 19.0f
    );

    drawText(renderer, 190, 282, "SIGNAL DIRECTORY");

    for (std::size_t i = 0; i < level.inputCount; ++i) {

        const std::string name( 1, static_cast<char>('A' + i));

        const std::string& description = level.inputDescriptions[i];

        const std::string label = name + " - " + (description.empty() ? "External input" : description);

        drawWrappedText(
            renderer,
            190,
            308.0f + static_cast<float>(i) * 25.0f,
            label,
            105,
            1
        );
    }

    const std::string outputLabel = "Y - " + (level.outputDescription.empty() ? std::string{ "Circuit output" } : level.outputDescription);

    drawWrappedText(
        renderer, 190, 522,
        outputLabel, 85, 2
    );

    checkSDL(SDL_RenderRect(renderer, &kBriefCloseButton));

    drawText(renderer, 949, 570, "CLOSE [I]" );
}



void Game::drawSaveSelect() {
    SDL_Renderer* renderer = renderer_.get();

    checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));

    drawText(renderer, 145, 85, "710 // PERSONNEL DATA TERMINAL");

    drawText(renderer, 145, 120, "SELECT SAVE FILE");

    drawText(renderer, 145, 148, "SELECT AN EMPTY SLOT TO BEGIN A NEW ASSIGNMENT");

    for (std::size_t i = 0; i < kSaveSlotCount; ++i) {
        const SDL_FRect row = saveSlotRect(i);

        std::string status = "EMPTY SLOT";

        std::error_code error;
        const bool exists = std::filesystem::exists(getSavePath(i), error);

        if (error) {
            status = "SAVE UNAVAILABLE";
        }
        else if (exists) {
            std::ifstream file{ getSavePath(i) };
            std::size_t verified{};

            if (file >> verified && verified <= levelManager_.levelCount()) {

                status = std::to_string(verified) + " WORK ORDERS VERIFIED";
            }
            else {
                status = "UNREADABLE SAVE";
            }
        }

        checkSDL(SDL_RenderRect(renderer, &row));

        drawText(
            renderer,
            row.x + 15,
            row.y + 17,
            "SAVE 0" + std::to_string(i + 1)
        );

        drawText(
            renderer,
            row.x + 15,
            row.y + 47,
            status
        );

        drawText(
            renderer,
            row.x + 650,
            row.y + 34,
            exists ? "LOAD" : "CREATE"
        );

        if (exists) {
            const SDL_FRect deleteButton = saveDeleteRect(i);

            checkSDL(SDL_RenderRect(renderer, &deleteButton));

            drawText(renderer, deleteButton.x + 46, deleteButton.y + 17, "DELETE");
        }
    }

    drawText(renderer, 145, 585, saveScreenMessage_);

    checkSDL(SDL_RenderRect(renderer, &kSaveExitButton));

    drawText(renderer, 620, 638, "EXIT");
}

void Game::drawDeleteConfirmation() {
    SDL_Renderer* renderer = renderer_.get();

    checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));

    drawText(renderer, 275, 160, "710 // FILE MANAGEMENT");

    drawText(renderer, 275, 215, "CONFIRM SAVE DELETION");

    if (pendingDeleteSlot_) {
        drawText(renderer, 275, 280, "DELETE SAVE 0" + std::to_string(*pendingDeleteSlot_ + 1) + "?");
    }

    drawText(renderer, 275, 320, "ALL PROGRESS AND ENGINEERING MERITS");

    drawText(renderer, 275, 340, "IN THIS SLOT WILL BE LOST.");

    checkSDL(SDL_RenderRect(renderer, &kConfirmDeleteButton));

    checkSDL(SDL_RenderRect(renderer, &kCancelDeleteButton));

    drawText(renderer, 430, 470, "DELETE SAVE");
    drawText(renderer, 755, 470, "CANCEL");
}



void Game::drawMainMenu() {
    SDL_Renderer* renderer = renderer_.get();

    checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));

    drawText(renderer, 135, 65, "710 // OFFSHORE MAINTENANCE TERMINAL");
    drawText(renderer, 135, 100, "WORK ORDER DIRECTORY");
    drawText(renderer, 135, 130, "PERSONNEL: APPRENTICE ENGINEER");

    drawText(
        renderer,
        135,
        156,
        "WORK ORDERS VERIFIED: " +
        std::to_string(completedLevelCount_)
    );

    // Show completed work orders and one newly authorised work order.
    const std::size_t visibleLevelCount = std::min(completedLevelCount_ + 1, levelManager_.levelCount());

    const std::size_t firstLevel = menuPage_ * kMenuPageSize;

    const std::size_t lastLevel = std::min(firstLevel + kMenuPageSize, visibleLevelCount);

    for (std::size_t i = firstLevel; i < lastLevel; ++i) {
        const std::size_t slot = i - firstLevel;
        const SDL_FRect rect = menuLevelRect(slot);

        const Level& level = levelManager_.levelAt(i);

        const bool completed = i < completedLevelCount_;

        checkSDL(SDL_SetRenderDrawColor(renderer, 15, 45, 27, 255));

        checkSDL(SDL_RenderFillRect(renderer, &rect));

        checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));

        checkSDL(SDL_RenderRect(renderer, &rect));

        drawText(
            renderer,
            rect.x + 12,
            rect.y + 9,
            "[" + level.id + "] " + level.title
        );

        std::string state = completed ? "710 VERIFIED" : "AUTHORISED";

        if (engineeringMerits_.contains(level.id)) {
            state += " / ENGINEERING MERIT";
        }

        drawText(renderer, rect.x + 12, rect.y + 31, state);
    }

    checkSDL(SDL_SetRenderDrawColor(renderer, 115, 220, 145, 255));

    checkSDL(SDL_RenderRect(renderer,&kMenuPreviousButton));

    checkSDL(SDL_RenderRect(renderer, &kMenuNextButton));

    drawText(renderer, 475, 568, "PREVIOUS PAGE");
    drawText(renderer, 690, 568, "NEXT PAGE");

    drawText(renderer, 135, 610, menuMessage_);

    checkSDL(SDL_RenderRect(renderer, &kSwitchSaveButton));

    drawText(renderer, 340, 658, "CHANGE SAVE");

    checkSDL(SDL_RenderRect(renderer, &kMenuExitButton));

    drawText(renderer, 615, 658, "EXIT");

    if (completedLevelCount_ == levelManager_.levelCount()) {
        drawText(
            renderer,
            135,
            175,
            "ALL WORK ORDERS VERIFIED"
        );
    }
}


void Game::openWorkOrder(
    std::size_t index,
    bool debugAccess
) {
    // Ordinary gameplay cannot open locked levels.
    if (!debugAccess && index > completedLevelCount_) {
        menuMessage_ = "ERROR 403: PREVIOUS WORK ORDER NOT VERIFIED";
        return;
    }

    if (!levelManager_.goToLevel(index)) {
        return;
    }

    screen_ = Screen::Puzzle;
    debugLevelOverride_ = debugAccess;

    selectedGate_.reset();
    placedGates_.clear();

    truthTablePage_ = 0;
    showBriefing_ = false;

    verificationStatus_ = VerificationStatus::Untested;

    const Level& level = levelManager_.currentLevel();

    logEvent("WORK ORDER " + level.id + " LOADED");

    if (!level.arrivalMessage.empty()) {
        setStatus(level.arrivalMessage);
    }
    else {
        setStatus("AWAITING COMPONENT PLACEMENT");
    }
}

void Game::handleMenuClick(float x, float y) {
    const std::size_t visibleLevelCount = std::min(completedLevelCount_ + 1, levelManager_.levelCount());

    const std::size_t firstLevel = menuPage_ * kMenuPageSize;

    for (std::size_t slot = 0; slot < kMenuPageSize; ++slot) {

        const std::size_t index = firstLevel + slot;

        if (index >= visibleLevelCount) {
            break;
        }

        if (containsPoint(menuLevelRect(slot), x, y)) {
            openWorkOrder(index);
            return;
        }
    }

    if (containsPoint(kMenuPreviousButton, x, y)) {
        if (menuPage_ > 0) {
            --menuPage_;
        }
        return;
    }

    if (containsPoint(kMenuNextButton, x, y)) {
        if ((menuPage_ + 1) * kMenuPageSize < visibleLevelCount) {

            ++menuPage_;
        }
        return;
    }

    if (containsPoint(kSwitchSaveButton, x, y)) {
        activeSaveSlot_.reset();

        screen_ = Screen::SaveSelect;
        saveScreenMessage_ = "SELECT A SAVE FILE";

        return;
    }

    if (containsPoint(kMenuExitButton, x, y)) {
        running_ = false;
    }
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
    
    drawWrappedText(
        renderer, 60, 164,
        level.objective, 31, 2
    );

    drawWrappedText(
        renderer, 60, 207,
        level.hint, 31, 2
    );

    checkSDL(SDL_RenderRect(renderer, &kBriefButton));

    drawText(
        renderer,
        70,
        245,
        "I: VIEW FULL BRIEFING"
    );

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
        /*/
        // Mark the terminal where the signal begins.
        const SDL_FRect terminal{
            position.x - 3.0f,
            position.y - 3.0f,
            6.0f,
            6.0f
        };

        checkSDL(SDL_RenderFillRect(renderer, &terminal));
        */
    }
    // ------------------------------

 
    // Draw circuit connections
    // ------------------------------
    checkSDL(SDL_SetRenderDrawColor(renderer, 75, 140, 95, 255));

    for (const auto& socket : level.sockets) {
        // Draw connections originating from external inputs.
        for (std::size_t i = 0; i < level.inputCount; ++i) {
            const std::string source(
                1,
                static_cast<char>('A' + i)
            );

            drawSignalConnections(
                renderer,
                level,
                source,
                i
            );
        }

        // Draw connections originating from gate outputs.
        for (std::size_t i = 0; i < level.sockets.size(); ++i) {
            drawSignalConnections(
                renderer,
                level,
                level.sockets[i].id,
                level.inputCount + i
            );
        }
    }

    // Redraw external input terminals after wires so the square appears on top of the line.
    for (std::size_t i = 0; i < level.inputCount; ++i) {
        const std::string name(1, static_cast<char>('A' + i));

        const SDL_FPoint position = sourcePosition(level, name);

        const SDL_FRect terminal{
            position.x - 3.0f,
            position.y - 3.0f,
            6.0f,
            6.0f
        };

        checkSDL(SDL_RenderFillRect(renderer, &terminal));
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

        const std::string label = it != placedGates_.end() ? gateName(it->second) : "EMPTY";

        // SDL_RenderDebugText uses 8x8 character cells.
        constexpr float characterWidth = 8.0f;
        constexpr float characterHeight = 8.0f;

        const float textWidth = static_cast<float>(label.size()) * characterWidth;

        drawText(
            renderer,
            socket.x + (kSocketWidth - textWidth) * 0.5f,
            socket.y + (kSocketHeight - characterHeight) * 0.5f,
            label
        );
    }

    // Draw input pin markers over the socket borders.
    for (const auto& socket : level.sockets) {
        for (std::size_t pin = 0; pin < socket.sources.size(); ++pin) {

            const SDL_FPoint position = inputPinPosition(socket, pin);

            const SDL_FRect inputMarker{
                position.x - 3.0f,
                position.y - 3.0f,
                6.0f,
                6.0f
            };

            checkSDL(SDL_RenderFillRect(renderer, &inputMarker));
        }
    }

    // Mark final output terminal, matching connection markers on external inputs.
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
        checkSDL(SDL_RenderRect(renderer, &kNextButton));

        if (levelManager_.hasNextLevel()) {
            drawText(renderer, 640, 548, "NEXT WORK ORDER");
        }
        else {
            drawText(renderer, 640, 548, "RETURN TO MENU");
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
    drawText(renderer, 60, 610, "SYSTEM LOG   [ PREVIOUS / ] NEXT");

    const std::size_t lastEntry = std::min(logFirstVisible_ + 3, systemLog_.size());

    for (std::size_t i = logFirstVisible_; i < lastEntry; ++i) {

        std::string line = "> " + systemLog_[i];

        // Prevent long messages overflowing the log panel.
        if (line.size() > 140) {
            line.resize(137);
            line += "...";
        }

        drawText(renderer, 60, 633.0f + static_cast<float>(i - logFirstVisible_) * 18.0f, line);
    }

    // Draw briefing on top of interface.
    if (showBriefing_) {
        drawWorkOrderBriefing();
    }
    // ------------------------------
}



void Game::render() {
    SDL_Renderer* renderer = renderer_.get();

    // Set background colour.
    checkSDL(SDL_SetRenderDrawColor(renderer, 8, 18, 12, 255));

    // Clear previous frame.
    checkSDL(SDL_RenderClear(renderer));
    
    // Draw puzzle interface.
    if (screen_ == Screen::SaveSelect) {
        drawSaveSelect();
    }
    else if (screen_ == Screen::DeleteConfirmation) {
        drawDeleteConfirmation();
    }
    else if (screen_ == Screen::MainMenu) {
        drawMainMenu();
    }
    else {
        drawPuzzlePreview();
    }

    // Present finished frame.
    checkSDL(SDL_RenderPresent(renderer));
}
