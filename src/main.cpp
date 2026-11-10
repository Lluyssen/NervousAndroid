#include <SDL3/SDL.h>
#include "MenuScreen.h"
#include "GameScreen.h"

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    ng::GameMode mode;
    {
        ng::MenuScreen menu;
        if (!menu.init()) { SDL_Quit(); return 1; }
        mode = menu.run();
    }   // le menu est dÃĐtruit ici : sa fenÃŠtre se ferme avant celle du jeu

    if (mode == ng::GameMode::None) { SDL_Quit(); return 0; }

    ng::GameScreen game(/*width*/ 20, /*height*/ 20);   // garde tes valeurs actuelles
    game.setMode(mode);

    if (mode == ng::GameMode::Multiplayer) {
        if (!game.connectOnline("127.0.0.1", 5000)) {
            SDL_Log("Connexion au serveur impossible");
            SDL_Quit();
            return 1;
        }
    }

    if (!game.init()) { SDL_Quit(); return 1; }
    game.run();

    SDL_Quit();
    return 0;
}