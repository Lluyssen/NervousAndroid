#include "GameScreen_Internal.h"

namespace ng {

void GameScreen::run() {
    while (running_ && !wantsMenu_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);

        if (paused_) {
            render();
            SDL_Delay(16);
            continue;
        }

        if (online() && connectionLost_ && !game_.isOver()) tryReconnect();
        else pollNetwork();

        if (isComputerTurn()) playComputerTurn();
        updateAnimations();
        render();
        SDL_Delay(16);
    }
    if (aiThread_.joinable()) aiThread_.join();
}

void GameScreen::handleEvent(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_QUIT:
            running_ = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
    if (e.button.button != SDL_BUTTON_LEFT) break;
    if (paused_) handlePauseClick(e.button.x, e.button.y);
    else         handleClick(e.button.x, e.button.y);
    break;
        case SDL_EVENT_KEY_DOWN:
    if (e.key.key == SDLK_ESCAPE) {
        paused_ = !paused_;
        pauseHover_ = -1;
        break;
    }
    if (paused_) break;                    // rien d'autre pendant la pause
    if (e.key.key == SDLK_SPACE || e.key.key == SDLK_RETURN) {
        if (online()) pollNetwork();
        if (canAct()) {
            if (online()) sendEndTurn();
            else afterStateChange(game_.endTurn());
        }
    }
    break;
        default:
            break;
    }
}

bool GameScreen::pixelToCell(float mx, float my, Pos* out) const {
    if (my < kHudHeight) return false;
    const int x = static_cast<int>(mx) / cellSize_;
    const int y = (static_cast<int>(my) - static_cast<int>(kHudHeight)) / cellSize_;
    if (x < 0 || y < 0 || x >= game_.board().width() || y >= game_.board().height())
        return false;
    *out = Pos{x, y};
    return true;
}

void GameScreen::handleClick(float mx, float my) {
    if (SDL_GetTicks() - startTicks_ < 250) return;

    // Etat reseau a jour avant de decider : evite le "double clic" du joueur 2
    // quand le START arrive juste avant le clic.
    if (online()) pollNetwork();

    if (game_.isOver() || isComputerTurn()) return;
    if (!canAct()) return;

    Pos p;
    if (!pixelToCell(mx, my, &p)) return;

    if (online()) { sendClick(p); return; }   // le serveur renvoie ACTION

    Result r = Result::WrongPhase;
    switch (game_.phase()) {
        case Phase::PlaceBaseP1:
        case Phase::PlaceBaseP2:
            r = game_.placeBase(p);
            break;
        case Phase::PlaySquares:
            r = game_.placeSquare(p);
            break;
        case Phase::RelocateBase: {
            Pos tl;
            r = game_.relocateBase(findLegalTl(p, &tl) ? tl : p);
            break;
        }
        case Phase::GameOver:
            return;
    }
    afterStateChange(r);
}

}  // namespace ng
