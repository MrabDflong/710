#pragma once

#include "LogicGate.h"
#include "LevelManager.h"

#include <SDL3/SDL.h>
#include <memory>
#include <optional>
#include <string>

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
    void handleMouseClick(float x, float y);
    void runPuzzleTests();

    void render();
	void drawPuzzlePreview();

    using WindowPtr = std::unique_ptr<
        SDL_Window, decltype(&SDL_DestroyWindow)>;

    using RendererPtr = std::unique_ptr<
        SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

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
    std::optional<GateType> placedGate_;

    VerificationStatus verificationStatus_{
        VerificationStatus::Untested
    };

    std::string statusMessage_{
        "AWAITING COMPONENT PLACEMENT"
    };

};
