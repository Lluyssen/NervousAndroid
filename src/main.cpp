#include <SDL3/SDL.h>
#include <memory>
#include "MenuScreen.h"
#include "GameScreen.h"
#include "../core/Game.h"

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    while (true) {
        ng::GameMode mode;
        {
            ng::MenuScreen menu;
            if (!menu.init()) break;
            mode = menu.run();
        }   // le menu est détruit ici : sa fenêtre se ferme avant celle du jeu

        if (mode == ng::GameMode::None) break;   // quitter

        std::unique_ptr<ng::GameScreen> game;

        if (mode == ng::GameMode::LoadSave) {
            int savedMode = 0;
            auto loaded = ng::Game::loadFromFile("save.txt", &savedMode);
            if (!loaded) {
                SDL_Log("Chargement impossible");
                continue;   // retour au menu
            }
            game = std::make_unique<ng::GameScreen>(
                std::move(*loaded), static_cast<ng::GameMode>(savedMode));
        } else {
            game = std::make_unique<ng::GameScreen>(20, 20);
            game->setMode(mode);
            if (mode == ng::GameMode::Multiplayer &&
                !game->connectOnline("127.0.0.1", 5000)) {
                SDL_Log("Connexion au serveur impossible");
                continue;   // retour au menu
            }
        }

        if (!game->init()) break;
        game->run();
        SDL_Log("retour du jeu, mode suivant...");
        // "Retour Menu" depuis la pause : on reboucle. Sinon on quitte.
        if (!game->wantsMenu()) break;
    }

    SDL_Quit();
    return 0;
}