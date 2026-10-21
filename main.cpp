#include "Game.h"

#include <SDL3/SDL_main.h>

#include <exception>
#include <iostream>

int main(int, char* []) {
    try {
        Game game;
        game.run();

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
