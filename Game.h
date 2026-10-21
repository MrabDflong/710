#pragma once

#include <SDL3/SDL.h>
#include <memory>

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
    void render();

    using WindowPtr = std::unique_ptr<
        SDL_Window, decltype(&SDL_DestroyWindow)>;

    using RendererPtr = std::unique_ptr<
        SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

    // Declaration order matters for destruction.
    SDLContext sdl_;

    WindowPtr window_{ nullptr, SDL_DestroyWindow };
    RendererPtr renderer_{ nullptr, SDL_DestroyRenderer };

    bool running_{ true };
};
