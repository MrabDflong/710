
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

// Manage SDL's initialisation and shutdown automatically.
class SDLContext {
public:
    SDLContext() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(
                std::string("SDL initialisation failed: ") +
                SDL_GetError()
            );
        }
    }

    ~SDLContext() {
        SDL_Quit();
    }

    SDLContext(const SDLContext&) = delete;
    SDLContext& operator=(const SDLContext&) = delete;
};

// Owning pointers that automatically destroy SDL resources.
using WindowPtr = std::unique_ptr<
    SDL_Window, decltype(&SDL_DestroyWindow)>;

using RendererPtr = std::unique_ptr<
    SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

int main(int, char* []) {
    try {
        SDLContext sdl;

        const std::string title = "710";

        WindowPtr window {
            SDL_CreateWindow(
                title.c_str(),
                1280,
                720,
                SDL_WINDOW_RESIZABLE
            ),
            SDL_DestroyWindow
        };

        if (!window) {
            throw std::runtime_error(
                std::string("Window creation failed: ") +
                SDL_GetError()
            );
        }

        RendererPtr renderer {
            SDL_CreateRenderer(window.get(), nullptr),
            SDL_DestroyRenderer
        };

        if (!renderer) {
            throw std::runtime_error(
                std::string("Renderer creation failed: ") +
                SDL_GetError()
            );
        }

        bool running = true;

        while (running) {
            SDL_Event event{};

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }

            SDL_SetRenderDrawColor(
                renderer.get(), 8, 18, 12, 255);

            SDL_RenderClear(renderer.get());
            SDL_RenderPresent(renderer.get());

            SDL_Delay(16);
        }

        // renderer, window and sdl clean up automatically.
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
