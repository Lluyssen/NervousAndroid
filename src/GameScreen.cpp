#include "GameScreen.h"
#include <algorithm>
#include <string>
#include <vector>

namespace ng {

namespace {
struct Color { Uint8 r, g, b, a; };
constexpr Color kBg        {20, 20, 28, 255};
constexpr Color kGridLine  {50, 50, 65, 255};
constexpr Color kP1        {60, 130, 230, 255};
constexpr Color kP1Base    {30, 80, 170, 255};
constexpr Color kP2        {230, 80, 80, 255};
constexpr Color kP2Base    {170, 40, 40, 255};
constexpr Color kHint      {255, 255, 120, 90};
constexpr Color kHover     {255, 255, 255, 70};
constexpr Color kText      {235, 235, 235, 255};

void setColor(SDL_Renderer* r, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
}  // namespace

GameScreen::GameScreen(int width, int height) : game_(width, height) {}

GameScreen::~GameScreen() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool GameScreen::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return false;
    }
    const Board& b = game_.board();
    // Adapte la taille des cases pour que les grandes grilles tiennent Ã  l'ÃĐcran.
    cellSize_ = std::max(6, std::min(28, 900 / std::max(b.width(), b.height())));
    int w = b.width() * cellSize_;
    int h = b.height() * cellSize_ + hudHeight_;

    if (!SDL_CreateWindowAndRenderer("Jeu de carres", w, h, 0, &window_, &renderer_)) {
        SDL_Log("CreateWindow: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    setMessage("J1 : placez votre base.");
    return true;
}

void GameScreen::run() {
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);
        render();
        SDL_Delay(16);
    }
}

void GameScreen::handleEvent(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_QUIT:
            running_ = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e.button.button == SDL_BUTTON_LEFT) handleClick(e.button.x, e.button.y);
            break;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.key == SDLK_ESCAPE) running_ = false;
            // Espace ou Entree : terminer le tour apres 1 carre.
            if (e.key.key == SDLK_SPACE || e.key.key == SDLK_RETURN) {
                Result r = game_.endTurn();
                setMessage(describe(r));
                dirty_ = true;
            }
            break;
        default: break;
    }
}

bool GameScreen::pixelToCell(float mx, float my, Pos* out) const {
    if (my < hudHeight_) return false;
    int x = static_cast<int>(mx) / cellSize_;
    int y = (static_cast<int>(my) - hudHeight_) / cellSize_;
    if (x < 0 || y < 0 || x >= game_.board().width() || y >= game_.board().height())
        return false;
    *out = Pos{x, y};
    return true;
}

// Reconstruit le cache des emplacements lÃĐgaux. AppelÃĐ uniquement quand
// l'ÃĐtat du jeu a changÃĐ (dirty_), jamais Ã  chaque frame.
void GameScreen::rebuildCache() {
    const Board& b = game_.board();
    const int W = b.width(), H = b.height();

    switch (game_.phase()) {
        case Phase::PlaceBaseP1:
        case Phase::PlaceBaseP2:
            legalCells_ = game_.legalInitialBasePlacements();
            break;
        case Phase::PlaySquares:
            legalCells_ = game_.legalSquarePlacements();
            break;
        case Phase::RelocateBase:
            legalCells_ = game_.legalBaseRelocations();
            break;
        default:
            legalCells_.clear();
            break;
    }

    // Une relocalisation couvre 2x2 cases, les autres phases 1 seule.
    const int span = (game_.phase() == Phase::RelocateBase) ? 2 : 1;
    legalTlMask_.assign(static_cast<size_t>(W) * H, 0);
    hintMask_.assign(static_cast<size_t>(W) * H, 0);

    for (const Pos& p : legalCells_) {
        legalTlMask_[static_cast<size_t>(p.y) * W + p.x] = 1;
        for (int dy = 0; dy < span; ++dy) {
            for (int dx = 0; dx < span; ++dx) {
                int x = p.x + dx, y = p.y + dy;
                if (x < W && y < H) hintMask_[static_cast<size_t>(y) * W + x] = 1;
            }
        }
    }
    dirty_ = false;
}

// Trouve le coin haut-gauche lÃĐgal dont l'emprise 2x2 couvre la case cliquÃĐe.
bool GameScreen::findLegalTl(Pos click, Pos* out) const {
    const int W = game_.board().width(), H = game_.board().height();
    for (int dy = 0; dy >= -1; --dy) {
        for (int dx = 0; dx >= -1; --dx) {
            int tx = click.x + dx, ty = click.y + dy;
            if (tx < 0 || ty < 0 || tx >= W || ty >= H) continue;
            if (legalTlMask_[static_cast<size_t>(ty) * W + tx]) {
                *out = Pos{tx, ty};
                return true;
            }
        }
    }
    return false;
}

void GameScreen::handleClick(float mx, float my) {
    if (game_.isOver()) return;
    Pos p;
    if (!pixelToCell(mx, my, &p)) return;

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
            if (dirty_) rebuildCache();
            Pos tl;
            if (findLegalTl(p, &tl)) r = game_.relocateBase(tl);
            else                     r = game_.relocateBase(p);  // renvoie l'erreur adaptÃĐe
            break;
        }
        case Phase::GameOver:
            return;
    }
    setMessage(describe(r));
    dirty_ = true;
}

void GameScreen::drawGrid() {
    const Board& b = game_.board();
    for (int y = 0; y < b.height(); ++y) {
        for (int x = 0; x < b.width(); ++x) {
            SDL_FRect rect{
                static_cast<float>(x * cellSize_),
                static_cast<float>(y * cellSize_ + hudHeight_),
                static_cast<float>(cellSize_ - 1),
                static_cast<float>(cellSize_ - 1)};

            Cell c = b.get(Pos{x, y});
            if (c == squareOf(Player::One))      setColor(renderer_, kP1);
            else if (c == squareOf(Player::Two)) setColor(renderer_, kP2);
            else if (c == baseOf(Player::One))   setColor(renderer_, kP1Base);
            else if (c == baseOf(Player::Two))   setColor(renderer_, kP2Base);
            else                                 setColor(renderer_, kGridLine);
            SDL_RenderFillRect(renderer_, &rect);
        }
    }
}

void GameScreen::drawHighlights() {
    if (game_.phase() == Phase::GameOver) return;
    if (dirty_) rebuildCache();

    const Board& b = game_.board();
    const int W = b.width(), H = b.height();

    // 1) Marqueurs : simple lecture du masque, aucun calcul de rÃĻgles.
    setColor(renderer_, kHint);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (!hintMask_[static_cast<size_t>(y) * W + x]) continue;
            SDL_FRect rect{
                static_cast<float>(x * cellSize_ + cellSize_ / 4),
                static_cast<float>(y * cellSize_ + hudHeight_ + cellSize_ / 4),
                static_cast<float>(cellSize_ / 2),
                static_cast<float>(cellSize_ / 2)};
            SDL_RenderFillRect(renderer_, &rect);
        }
    }

    // 2) AperÃ§u 2x2 de la base sous la souris (lookup O(1)).
    if (game_.phase() == Phase::RelocateBase) {
        float mx, my;
        SDL_GetMouseState(&mx, &my);
        Pos hover, tl;
        if (pixelToCell(mx, my, &hover) && findLegalTl(hover, &tl)) {
            setColor(renderer_, kHover);
            SDL_FRect r{
                static_cast<float>(tl.x * cellSize_),
                static_cast<float>(tl.y * cellSize_ + hudHeight_),
                static_cast<float>(cellSize_ * 2 - 1),
                static_cast<float>(cellSize_ * 2 - 1)};
            SDL_RenderFillRect(renderer_, &r);
        }
    }
}

void GameScreen::drawHud() {
    setColor(renderer_, kText);
    auto line = [&](float y, const std::string& s) {
        SDL_RenderDebugText(renderer_, 8.0f, y, s.c_str());
    };

    std::string hp = "J1 PV: " + std::to_string(game_.hp(Player::One)) +
                     "   J2 PV: " + std::to_string(game_.hp(Player::Two));
    line(6, hp);

    std::string status;
    if (game_.isOver()) {
        status = std::string("Partie terminee. Vainqueur : ") +
                 (game_.winner() == Player::One ? "J1" : "J2");
    } else {
        status = std::string("Tour de ") +
                 (game_.current() == Player::One ? "J1" : "J2");
        switch (game_.phase()) {
            case Phase::PlaceBaseP1:
            case Phase::PlaceBaseP2: status += " - placer la base"; break;
            case Phase::PlaySquares: status += " - poser des carres (Espace = fin de tour)"; break;
            case Phase::RelocateBase: status += " - DEPLACER LA BASE"; break;
            default: break;
        }
    }
    line(20, status);
    line(34, message_);
}

void GameScreen::render() {
    setColor(renderer_, kBg);
    SDL_RenderClear(renderer_);
    drawGrid();
    drawHighlights();
    drawHud();
    SDL_RenderPresent(renderer_);
}

}  // namespace ng