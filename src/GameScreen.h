#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "../core/Game.h"

namespace ng {

    class GameScreen {
    public:
        GameScreen(int width, int height);
        ~GameScreen();

        bool init();
        void run();

    private:

            bool dirty_ = true;
    std::vector<Pos>  legalCells_;      // liste brute renvoyÃĐe par Game
    std::vector<char> legalTlMask_;     // lookup O(1) : tl lÃĐgal ?
    std::vector<char> hintMask_;        // cases Ã  marquer visuellement
    void rebuildCache();

        bool findLegalTl(Pos click, Pos* out) const;

        void handleEvent(const SDL_Event& e);
        void handleClick(float mx, float my);
        void render();

        void drawGrid();
        void drawHighlights();
        void drawHud();

        bool pixelToCell(float mx, float my, Pos* out) const;
        void setMessage(const std::string& m) { message_ = m; }

        Game game_;
        SDL_Window* window_ = nullptr;
        SDL_Renderer* renderer_ = nullptr;

        int cellSize_ = 24;
        int hudHeight_ = 70;
        bool running_ = true;
        std::string message_;
    };

}  // namespace ng