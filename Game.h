#pragma once

#include "LogicGate.h"
#include "LevelManager.h"

#include <cstddef>
#include <SDL3/SDL.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Manages SDL's lifecycle using RAII
class SDLContext {
public:
    SDLContext();
    ~SDLContext();

    SDLContext(const SDLContext&) = delete;
    SDLContext& operator=(const SDLContext&) = delete;
};

class Game {
public:
    Game();

    void run();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

private:
    void handleEvents();
    void handleMouseClick(float x, float y, bool removeGate);
    void runPuzzleTests();

    void render();
    void drawPuzzlePreview();
    void drawWorkOrderBriefing();
    void drawMainMenu();
    void drawSaveSelect();
    void drawDeleteConfirmation();

    void handleMenuClick(float x, float y);
    void handleSaveSelectClick(float x, float y);
    void handleDeleteConfirmationClick(float x, float y);

    void selectSaveSlot(std::size_t slot);
    void openWorkOrder(std::size_t index, bool debugAccess = false);

    bool loadProgress();
    bool saveProgress() const;

    void logEvent(std::string message);
    void setStatus(std::string message);

    using WindowPtr = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;

    using RendererPtr = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

    // Declaration order matters for destruction.
    SDLContext sdl_;

    LevelManager levelManager_;

    WindowPtr window_{ nullptr, SDL_DestroyWindow };
    RendererPtr renderer_{ nullptr, SDL_DestroyRenderer };

    bool running_{ true };


    enum class VerificationStatus {
        Untested,
        Passed,
        Failed
    };

    std::optional<GateType> selectedGate_;
    
    // One placed gate per socket ID.
    std::unordered_map<std::string, GateType> placedGates_;

    enum class Screen {
        SaveSelect,
        DeleteConfirmation,
        MainMenu,
        Puzzle
    };

    Screen screen_{ Screen::SaveSelect };

    static constexpr std::size_t kSaveSlotCount = 3;

    std::optional<std::size_t> activeSaveSlot_;
    std::optional<std::size_t> pendingDeleteSlot_;

    std::string saveScreenMessage_{ "SELECT A SAVE FILE" };

    std::size_t completedLevelCount_{ 0 };

    std::size_t menuPage_{ 0 };
    std::string menuMessage_{ "SELECT AN AUTHORISED WORK ORDER" };

    // Debug navigation must not change saved progression.
    bool debugLevelOverride_{ false };

    std::size_t truthTablePage_{ 0 };
    bool showBriefing_{ false };

    // Work orders which earned efficiency award.
    std::unordered_set<std::string> engineeringMerits_;

    // Recent and historical terminal messages.
    std::vector<std::string> systemLog_;
    std::size_t logFirstVisible_{ 0 };

    VerificationStatus verificationStatus_{
        VerificationStatus::Untested
    };

    std::string statusMessage_{
        "AWAITING COMPONENT PLACEMENT"
    };

};
