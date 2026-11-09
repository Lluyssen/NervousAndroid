#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "../core/Game.h"
#include "MenuScreen.h"

namespace ng {

    class GameScreen {
    public:
        GameScreen(int width, int height);
        ~GameScreen();

        bool init();
        void run();
        void setMode(GameMode m) { mode_ = m; }

    private:

            bool dirty_ = true;
    std::vector<Pos>  legalCells_;      // liste brute renvoyÃĐe par Game
    std::vector<char> legalTlMask_;     // lookup O(1) : tl lÃĐgal ?
    std::vector<char> hintMask_;        // cases Ã  marquer visuellement

    GameMode mode_ = GameMode::LocalOneVsOne;
    Uint64 startTicks_ = 0;
    
    void playComputerTurn();

    
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
        int hudHeight_ = 112;
        bool running_ = true;
        std::string message_;

            // --- Interface moderne ---
    int   maxHp_ = 1;                // PV max, lu Ã  l'init
    float shownHp_[2] = {0.f, 0.f};  // PV affichÃĐs (animÃĐs)
    Uint64 lastTicks_ = 0;
    float  time_ = 0.f;              // temps cumulÃĐ pour les pulsations

    float lostFlash_[2] = {0.f, 0.f};   // flash rouge au PV perdu
    int   prevHp_[2]    = {-1, -1};     // PV vus Ã  la frame prÃĐcÃĐdente

    void drawRoundedRect(float x, float y, float w, float h, float radius);
    void drawText(float x, float y, const std::string& s, float scale);
    void drawPlayerCard(Player pl, float x, float y, float w, float h);
    void drawStatusBar(float w);
    void drawGameOver();
    void updateAnimations();
    std::string playerName(Player pl) const;
        void drawMessageToast();
    };

}  // namespace ng