#include "Game.h"

#include <stdexcept>
#include <string>

// Helpers used only within Game.cpp.
namespace {

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

    // Socket where a gate will be placed.
    const SDL_FRect gateSocket{ 615, 295, 160, 85 };

    checkSDL(SDL_RenderRect(renderer, &gateSocket));

    // Two input wires.
    checkSDL(SDL_RenderLine(
        renderer, 435, 298, 615, 320
    ));

    checkSDL(SDL_RenderLine(
        renderer, 435, 418, 615, 350
    ));

    // Output wire.
    checkSDL(SDL_RenderLine(
        renderer, 775, 338, 875, 338
    ));

    // Labels for circuit inputs and output.
    drawText(renderer, 398, 291, "A");
    drawText(renderer, 398, 411, "B");
    drawText(renderer, 680, 333, "EMPTY");
    drawText(renderer, 885, 332, "Y");
    // ------------------------------



    // Gate inventory
    // ------------------------------
    drawText(renderer, 985, 130, "AVAILABLE GATES");

    const SDL_FRect andBox{ 990, 235, 215, 50 };
    const SDL_FRect orBox{ 990, 310, 215, 50 };
    const SDL_FRect notBox{ 990, 385, 215, 50 };

    checkSDL(SDL_RenderRect(renderer, &andBox));
    checkSDL(SDL_RenderRect(renderer, &orBox));
    checkSDL(SDL_RenderRect(renderer, &notBox));

    drawText(renderer, 1010, 255, "AND");
    drawText(renderer, 1010, 330, "OR");
    drawText(renderer, 1010, 405, "NOT");
    // ------------------------------


   
    // System log
    // ------------------------------
    drawText(renderer, 60, 610, "SYSTEM LOG");
    drawText(renderer, 60, 640, "> WORK ORDER 01 LOADED");
    drawText(renderer, 60, 660, "> AWAITING COMPONENT PLACEMENT");
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
