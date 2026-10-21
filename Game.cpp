#include "Game.h"

#include <stdexcept>
#include <string>

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

void Game::render() {
    if (!SDL_SetRenderDrawColor(renderer_.get(), 8, 18, 12, 255)) {
        throw std::runtime_error(SDL_GetError());
    }

    if (!SDL_RenderClear(renderer_.get())) {
        throw std::runtime_error(SDL_GetError());
    }

    if (!SDL_RenderPresent(renderer_.get())) {
        throw std::runtime_error(SDL_GetError());
    }
}
