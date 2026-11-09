#include <SDL3/SDL.h>
#include "MenuScreen.h"
#include "GameScreen.h"

int main() {
    // SDL est initialisÃĐ une seule fois pour tout le programme.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    ng::GameMode mode = ng::GameMode::None;
    {
        ng::MenuScreen menu;
        if (!menu.init()) {
            SDL_Quit();
            return 1;
        }
        mode = menu.run();
    }   // fenÃŠtre du menu dÃĐtruite ici

    if (mode == ng::GameMode::None) {   // fenÃŠtre fermÃĐe / Echap
        SDL_Quit();
        return 0;
    }

    int result = 0;
    {
        ng::GameScreen screen(/*width*/ 30, /*height*/ 30);   // tes valeurs actuelles
        screen.setMode(mode);
        if (!screen.init()) {
            result = 1;
        } else {
            screen.run();
        }
    }   // fenÃŠtre du jeu dÃĐtruite ici

    SDL_Quit();
    return result;

}