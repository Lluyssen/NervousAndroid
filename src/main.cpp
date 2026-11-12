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
        bool load = false;
        {
            ng::MenuScreen menu;
            if (!menu.init()) break;
            mode = menu.run();
            load = menu.wantsLoad();   // a lire avant la destruction du menu
        }   // le menu est détruit ici : sa fenêtre se ferme avant celle du jeu

        if (mode == ng::GameMode::None) break;   // quitter

        std::unique_ptr<ng::GameScreen> game;

        if (load) {
            // "Charger" : 1 contre 1 ou Heberger. Le mode choisi dans le menu
            // (mode) fait foi ; le mode stocké dans la sauvegarde est ignoré.
            int savedMode = 0;
            auto loaded = ng::Game::loadFromFile("save.txt", &savedMode);
            if (!loaded) {
                SDL_Log("Chargement impossible");
                continue;   // retour au menu
            }
            game = std::make_unique<ng::GameScreen>(std::move(*loaded), mode);
        } else {
            game = std::make_unique<ng::GameScreen>(20, 20);
            game->setMode(mode);
        }

        // Reseau : Host et Join passent tous deux par connectOnline.
        if (mode == ng::GameMode::Host || mode == ng::GameMode::Join) {
            if (!game->connectOnline("127.0.0.1", 5000)) {
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