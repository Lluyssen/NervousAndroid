#include "GameScreen.h"
#include "Rules.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <cstdlib>

namespace ng {

namespace {
struct Color { Uint8 r, g, b, a; };

// --- Palette moderne ---
constexpr Color kBg        {16, 18, 26, 255};
constexpr Color kPanel     {26, 29, 42, 255};
constexpr Color kPanelHi   {36, 40, 58, 255};
constexpr Color kGridCell  {34, 38, 54, 255};
constexpr Color kText      {236, 239, 248, 255};
constexpr Color kTextDim   {140, 148, 172, 255};
constexpr Color kP1        {64, 140, 255, 255};
constexpr Color kP1Dark    {30, 80, 180, 255};
constexpr Color kP2        {255, 92, 108, 255};
constexpr Color kP2Dark    {180, 44, 64, 255};
constexpr Color kHint      {255, 236, 130, 95};
constexpr Color kHover     {255, 255, 255, 80};
constexpr Color kGood      {80, 220, 140, 255};
constexpr Color kCard      {28, 32, 48, 255};   // fond des cartes joueur

void setColor(SDL_Renderer* r, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
Color withAlpha(Color c, Uint8 a) { return Color{c.r, c.g, c.b, a}; }
Color mix(Color a, Color b, float t) {
    auto m = [&](Uint8 x, Uint8 y) {
        return static_cast<Uint8>(x + (y - x) * t);
    };
    return Color{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}
Color playerColor(Player p)     { return p == Player::One ? kP1 : kP2; }
Color playerDarkColor(Player p) { return p == Player::One ? kP1Dark : kP2Dark; }
}  // namespace

GameScreen::GameScreen(int width, int height) : game_(width, height) {}

GameScreen::~GameScreen() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
}

bool GameScreen::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return false;
    }
    const Board& b = game_.board();
    cellSize_ = std::max(6, std::min(28, 900 / std::max(b.width(), b.height())));

    // Largeur minimale pour afficher correctement les deux cartes joueurs.
    int w = std::max(b.width() * cellSize_, 560);
    int h = b.height() * cellSize_ + hudHeight_;

    if (!SDL_CreateWindowAndRenderer("Jeu de carres", w, h, 0, &window_, &renderer_)) {
        SDL_Log("CreateWindow: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // PV max = PV de dÃĐpart (les bases viennent d'ÃŠtre crÃĐÃĐes).
    maxHp_ = std::max(1, std::max(game_.hp(Player::One), game_.hp(Player::Two)));
    prevHp_[0] = game_.hp(Player::One);
    prevHp_[1] = game_.hp(Player::Two);
    lastTicks_ = SDL_GetTicks();

    SDL_RaiseWindow(window_);
SDL_PumpEvents();
SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
startTicks_ = SDL_GetTicks();  

    setMessage("J1 : placez votre base.");
    return true;
}

void GameScreen::playComputerTurn() {
    static Uint64 nextMove = 0;
    if (SDL_GetTicks() < nextMove) return;      // petit dÃĐlai pour voir le coup
    nextMove = SDL_GetTicks() + 350;

    Result r = Result::WrongPhase;
    switch (game_.phase()) {
        case Phase::PlaceBaseP2: {
            auto cells = game_.legalInitialBasePlacements();
            if (!cells.empty()) r = game_.placeBase(cells[rand() % cells.size()]);
            break;
        }
        case Phase::RelocateBase: {
            auto cells = game_.legalBaseRelocations();
            if (!cells.empty()) r = game_.relocateBase(cells[rand() % cells.size()]);
            break;
        }
        case Phase::PlaySquares: {
            auto cells = game_.legalSquarePlacements();
            if (cells.empty()) { r = game_.endTurn(); break; }
            // Heuristique : la pose la plus proche de la base adverse.
            Pos target;
            if (!rules::findBase(game_.board(), Player::One, &target)) target = cells[0];
            Pos best = cells[0];
            int bestD = 1 << 30;
            for (const Pos& c : cells) {
                int d = std::abs(c.x - target.x) + std::abs(c.y - target.y);
                if (d < bestD) { bestD = d; best = c; }
            }
            r = game_.placeSquare(best);
            break;
        }
        default: return;
    }
    setMessage(describe(r));
    dirty_ = true;
}

void GameScreen::run() {
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);

        if (mode_ == GameMode::VsComputer && !game_.isOver() &&
            game_.current() == Player::Two)
            playComputerTurn();

        updateAnimations();
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

void GameScreen::rebuildCache() {
    const Board& b = game_.board();
    const int W = b.width(), H = b.height();
    legalCells_.clear();
    legalTlMask_.assign(static_cast<size_t>(W) * H, 0);
    hintMask_.assign(static_cast<size_t>(W) * H, 0);

    const Phase ph = game_.phase();
    const bool isBase = (ph == Phase::PlaceBaseP1 || ph == Phase::PlaceBaseP2 ||
                         ph == Phase::RelocateBase);

    switch (ph) {
        case Phase::PlaceBaseP1:
        case Phase::PlaceBaseP2:
            legalCells_ = game_.legalInitialBasePlacements();
            break;
        case Phase::RelocateBase:
            legalCells_ = game_.legalBaseRelocations();
            break;
        case Phase::PlaySquares:
            legalCells_ = game_.legalSquarePlacements();
            break;
        default:
            break;
    }

    for (const Pos& p : legalCells_) {
        if (p.x < 0 || p.y < 0 || p.x >= W || p.y >= H) continue;
        legalTlMask_[static_cast<size_t>(p.y) * W + p.x] = 1;

        if (isBase) {
            // Une base occupe 2x2 : on marque les 4 cases couvertes,
            // sinon la derniÃĻre ligne/colonne n'affiche jamais de rond.
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    int cx = p.x + dx, cy = p.y + dy;
                    if (cx < W && cy < H)
                        hintMask_[static_cast<size_t>(cy) * W + cx] = 1;
                }
            }
        } else {
            hintMask_[static_cast<size_t>(p.y) * W + p.x] = 1;
        }
    }
    dirty_ = false;
}

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
    if (SDL_GetTicks() - startTicks_ < 250) return;
    if (mode_ == GameMode::VsComputer && game_.current() == Player::Two) return;
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
            else                     r = game_.relocateBase(p);
            break;
        }
        case Phase::GameOver:
            return;
    }
    setMessage(describe(r));
    dirty_ = true;
}

// ---------------------------------------------------------------------------
//  Primitives de dessin
// ---------------------------------------------------------------------------

// Rectangle arrondi : un corps central + des bandes horizontales pour les coins.
void GameScreen::drawRoundedRect(float x, float y, float w, float h, float radius) {
    radius = std::min(radius, std::min(w, h) * 0.5f);
    if (radius < 1.f) {
        SDL_FRect r{x, y, w, h};
        SDL_RenderFillRect(renderer_, &r);
        return;
    }
    // Corps central.
    SDL_FRect mid{x, y + radius, w, h - 2 * radius};
    SDL_RenderFillRect(renderer_, &mid);
    // Coins : une bande par ligne de pixel, largeur selon le cercle.
    int rr = static_cast<int>(radius);
    for (int i = 0; i < rr; ++i) {
        float dy = radius - i - 0.5f;
        float dx = radius - std::sqrt(std::max(0.f, radius * radius - dy * dy));
        SDL_FRect top{x + dx, y + i, w - 2 * dx, 1.f};
        SDL_FRect bot{x + dx, y + h - 1 - i, w - 2 * dx, 1.f};
        SDL_RenderFillRect(renderer_, &top);
        SDL_RenderFillRect(renderer_, &bot);
    }
}

void GameScreen::drawText(float x, float y, const std::string& s, float scale) {
    SDL_SetRenderScale(renderer_, scale, scale);
    SDL_RenderDebugText(renderer_, x / scale, y / scale, s.c_str());
    SDL_SetRenderScale(renderer_, 1.f, 1.f);
}

std::string GameScreen::playerName(Player pl) const {
    return pl == Player::One ? "Joueur 1" : "Joueur 2";
}

void GameScreen::updateAnimations() {
    Uint64 now = SDL_GetTicks();
    float dt = static_cast<float>(now - lastTicks_) / 1000.f;
    lastTicks_ = now;
    time_ += dt;

    // DÃĐtecte la perte d'un PV et lance un flash qui s'estompe en ~0,6 s.
    for (int i = 0; i < 2; ++i) {
        Player pl = (i == 0) ? Player::One : Player::Two;
        int hp = game_.hp(pl);
        if (prevHp_[i] >= 0 && hp < prevHp_[i]) lostFlash_[i] = 1.f;
        prevHp_[i] = hp;
        lostFlash_[i] = std::max(0.f, lostFlash_[i] - dt / 0.6f);
    }
}

// ---------------------------------------------------------------------------
//  Plateau
// ---------------------------------------------------------------------------

void GameScreen::drawGrid() {
    const Board& b = game_.board();
    const float gap = (cellSize_ >= 12) ? 1.f : 0.f;
    const float rad = (cellSize_ >= 16) ? 3.f : 0.f;

    for (int y = 0; y < b.height(); ++y) {
        for (int x = 0; x < b.width(); ++x) {
            float fx = static_cast<float>(x * cellSize_);
            float fy = static_cast<float>(y * cellSize_ + hudHeight_);
            float sz = static_cast<float>(cellSize_) - gap;

            Cell c = b.get(Pos{x, y});
            if (c == squareOf(Player::One))      setColor(renderer_, kP1);
            else if (c == squareOf(Player::Two)) setColor(renderer_, kP2);
            else if (c == baseOf(Player::One))   setColor(renderer_, kP1Dark);
            else if (c == baseOf(Player::Two))   setColor(renderer_, kP2Dark);
            else                                 setColor(renderer_, kGridCell);

            drawRoundedRect(fx, fy, sz, sz, rad);
        }
    }

    // Liseret lumineux sur les bases pour bien les distinguer.
    for (int y = 0; y < b.height(); ++y) {
        for (int x = 0; x < b.width(); ++x) {
            Cell c = b.get(Pos{x, y});
            bool b1 = (c == baseOf(Player::One));
            bool b2 = (c == baseOf(Player::Two));
            if (!b1 && !b2) continue;
            setColor(renderer_, withAlpha(b1 ? kP1 : kP2, 150));
            float m = std::max(1.f, cellSize_ * 0.18f);
            SDL_FRect r{x * (float)cellSize_ + m,
                        y * (float)cellSize_ + hudHeight_ + m,
                        cellSize_ - 2 * m - 1.f, cellSize_ - 2 * m - 1.f};
            SDL_RenderFillRect(renderer_, &r);
        }
    }
}

void GameScreen::drawHighlights() {
    if (game_.phase() == Phase::GameOver) return;
    if (dirty_) rebuildCache();

    const Board& b = game_.board();
    const int W = b.width(), H = b.height();

    // Les marqueurs respirent doucement (pulsation sinusoÃŊdale).
    float pulse = 0.65f + 0.35f * std::sin(time_ * 4.f);
    Color hint = withAlpha(kHint, static_cast<Uint8>(kHint.a * pulse + 40));
    setColor(renderer_, hint);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (!hintMask_[static_cast<size_t>(y) * W + x]) continue;
            float m = cellSize_ * 0.30f;
            drawRoundedRect(x * (float)cellSize_ + m,
                            y * (float)cellSize_ + hudHeight_ + m,
                            cellSize_ - 2 * m, cellSize_ - 2 * m,
                            (cellSize_ - 2 * m) * 0.5f);
        }
    }

    // --- AperÃ§u sous la souris ---
    const Phase ph = game_.phase();
    const bool isBase = (ph == Phase::PlaceBaseP1 || ph == Phase::PlaceBaseP2 ||
                         ph == Phase::RelocateBase);
    const bool isSquare = (ph == Phase::PlaySquares);
    if (!isBase && !isSquare) return;

    float mx, my;
    SDL_GetMouseState(&mx, &my);
    Pos hover;
    if (!pixelToCell(mx, my, &hover)) return;

    Color pc = playerColor(game_.current());

    if (isBase) {
        // Base 2x2 : on cherche le coin haut-gauche lÃĐgal couvrant la case survolÃĐe.
        Pos tl;
        if (findLegalTl(hover, &tl)) {
            setColor(renderer_, withAlpha(pc, 150));
            drawRoundedRect(tl.x * (float)cellSize_,
                            tl.y * (float)cellSize_ + hudHeight_,
                            cellSize_ * 2 - 1.f, cellSize_ * 2 - 1.f, 4.f);
            // Contour pour bien voir l'emprise.
            setColor(renderer_, withAlpha(Color{255, 255, 255, 255}, 120));
            SDL_FRect o{tl.x * (float)cellSize_,
                        tl.y * (float)cellSize_ + hudHeight_,
                        cellSize_ * 2 - 1.f, cellSize_ * 2 - 1.f};
            SDL_RenderRect(renderer_, &o);
        } else {
            // Emplacement illÃĐgal : case survolÃĐe en rouge.
            setColor(renderer_, Color{255, 80, 80, 140});
            SDL_FRect r{hover.x * (float)cellSize_,
                        hover.y * (float)cellSize_ + hudHeight_,
                        cellSize_ - 1.f, cellSize_ - 1.f};
            SDL_RenderFillRect(renderer_, &r);
        }
    } else {
        // CarrÃĐ 1x1 : lÃĐgal si la case survolÃĐe est dans le masque de pose.
        bool legal = hintMask_[static_cast<size_t>(hover.y) * W + hover.x] != 0;
        setColor(renderer_, legal ? withAlpha(pc, 160) : Color{255, 80, 80, 140});
        SDL_FRect r{hover.x * (float)cellSize_,
                    hover.y * (float)cellSize_ + hudHeight_,
                    cellSize_ - 1.f, cellSize_ - 1.f};
        SDL_RenderFillRect(renderer_, &r);
    }
}

// ---------------------------------------------------------------------------
//  HUD moderne
// ---------------------------------------------------------------------------

void GameScreen::drawPlayerCard(Player pl, float x, float y, float w, float h) {
    const int idx = (pl == Player::One) ? 0 : 1;
    const bool active = (game_.current() == pl) && !game_.isOver();
    Color pc = playerColor(pl);

    // Fond de la carte
    setColor(renderer_, kCard);
    drawRoundedRect(x, y, w, h, 12.f);

    // Liseret de couleur si c'est son tour
    if (active) {
        float pulse = 0.6f + 0.4f * std::sin(time_ * 4.f);
        setColor(renderer_, withAlpha(pc, static_cast<Uint8>(120 + 100 * pulse)));
        SDL_FRect o{x, y, w, h};
        SDL_RenderRect(renderer_, &o);
    }

    // Pastille + nom, centrÃĐs verticalement (texte ~16 px de haut Ã  l'ÃĐchelle 2)
    setColor(renderer_, pc);
    drawRoundedRect(x + 14, y + (h - 14.f) * 0.5f, 14.f, 14.f, 7.f);
    setColor(renderer_, kText);
    drawText(x + 38, y + (h - 16.f) * 0.5f, playerName(pl), 2.0f);

    // --- Points de vie : un carrÃĐ par PV, Ã  droite, centrÃĐs verticalement ---
    const int hp = game_.hp(pl);
    const int total = std::max(1, maxHp_);
    const float gap = 6.f;
    const float box = std::min(20.f, h - 20.f);
    const float rowW = total * box + (total - 1) * gap;

    // On rÃĐserve la place du compteur "4/4" tout Ã  droite.
    const float counterW = 50.f;
    const float bx = x + w - 14.f - counterW - rowW;
    const float by = y + (h - box) * 0.5f;

    for (int i = 0; i < total; ++i) {
        const float cx = bx + i * (box + gap);
        const bool alive = i < hp;

        if (alive) {
            // PV restant : couleur du joueur + petit reflet
            setColor(renderer_, pc);
            drawRoundedRect(cx, by, box, box, 5.f);
            setColor(renderer_, withAlpha(Color{255, 255, 255, 255}, 60));
            drawRoundedRect(cx + 3, by + 3, box - 6, (box - 6) * 0.45f, 3.f);
        } else {
            // PV perdu : case vide sombre, contour discret
            setColor(renderer_, Color{255, 255, 255, 18});
            drawRoundedRect(cx, by, box, box, 5.f);
            setColor(renderer_, Color{255, 255, 255, 45});
            SDL_FRect o{cx, by, box, box};
            SDL_RenderRect(renderer_, &o);
        }
    }

    // Flash rouge sur le dernier PV perdu (dÃĐcroÃŪt en ~0,6 s)
    if (lostFlash_[idx] > 0.f) {
        const int lostIdx = hp;  // le carrÃĐ qui vient de s'ÃĐteindre
        if (lostIdx >= 0 && lostIdx < total) {
            const float cx = bx + lostIdx * (box + gap);
            setColor(renderer_, Color{255, 70, 70,
                     static_cast<Uint8>(200 * lostFlash_[idx])});
            drawRoundedRect(cx, by, box, box, 5.f);
        }
    }

    // Compteur numÃĐrique Ã  l'extrÃŠme droite, centrÃĐ verticalement
    std::string t = std::to_string(hp) + "/" + std::to_string(total);
    setColor(renderer_, kTextDim);
    drawText(x + w - 14.f - counterW + 6.f, y + (h - 12.f) * 0.5f, t, 1.5f);
}

void GameScreen::drawStatusBar(float w) {
    const float y = 78.f, h = 28.f, margin = 12.f;
    Player cur = game_.current();
    Color accent = game_.isOver() ? kGood : playerColor(cur);

    std::string phase;
    if (game_.isOver()) {
        phase = "Partie terminee";
    } else {
        switch (game_.phase()) {
            case Phase::PlaceBaseP1:
            case Phase::PlaceBaseP2:  phase = "Placez votre base"; break;
            case Phase::PlaySquares:  phase = "Posez vos carres  -  Espace : fin de tour"; break;
            case Phase::RelocateBase: phase = "Deplacez votre base"; break;
            default: break;
        }
        phase = "Tour de " + playerName(cur) + "  |  " + phase;
    }

    // Largeur du texte : 8 px par caractÃĻre Ã  l'ÃĐchelle 1, multipliÃĐ par l'ÃĐchelle du texte.
    const float textScale = 1.5f;
    const float textW = static_cast<float>(phase.size()) * 8.f * textScale;
    const float textH = 8.f * textScale;

    // Largeur du bandeau = texte + marges, bornÃĐe Ã  la largeur disponible.
    const float padX = 24.f;
    const float barW = std::min(w - 2 * margin, textW + 2 * padX);
    const float barX = (w - barW) * 0.5f;

    // Fond du bandeau.
    setColor(renderer_, kPanel);
    drawRoundedRect(barX, y, barW, h, 10.f);
    // Liseret colorÃĐ selon le joueur actif, Ã  gauche du bandeau.
    setColor(renderer_, accent);
    drawRoundedRect(barX, y, 6.f, h, 3.f);

    // Texte centrÃĐ horizontalement et verticalement dans le bandeau.
    setColor(renderer_, kText);
    drawText(barX + (barW - textW) * 0.5f, y + (h - textH) * 0.5f, phase, textScale);
}

void GameScreen::drawHud() {
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window_, &winW, &winH);
    const float w = static_cast<float>(winW);

    // Fond du HUD.
    setColor(renderer_, kBg);
    SDL_FRect bg{0, 0, w, static_cast<float>(hudHeight_)};
    SDL_RenderFillRect(renderer_, &bg);

    // Deux cartes cÃīte Ã  cÃīte.
    const float margin = 12.f, gap = 12.f, cardY = 8.f, cardH = 66.f;
    const float cardW = (w - 2 * margin - gap) / 2.f;
    drawPlayerCard(Player::One, margin, cardY, cardW, cardH);
    drawPlayerCard(Player::Two, margin + cardW + gap, cardY, cardW, cardH);

    drawStatusBar(w);
}

// Superposition de fin de partie.
void GameScreen::drawGameOver() {
    if (!game_.isOver()) return;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window_, &winW, &winH);
    const float w = static_cast<float>(winW), h = static_cast<float>(winH);

    // Voile sombre.
    setColor(renderer_, Color{8, 9, 14, 200});
    SDL_FRect veil{0, 0, w, h};
    SDL_RenderFillRect(renderer_, &veil);

    const Player win = game_.winner();
    const Color col = playerColor(win);

    const float cw = std::min(w - 40.f, 460.f), ch = 190.f;
    const float cx = (w - cw) / 2.f, cy = (h - ch) / 2.f;

    // Halo pulsant + carte.
    float pulse = 0.6f + 0.4f * std::sin(time_ * 3.f);
    setColor(renderer_, withAlpha(col, static_cast<Uint8>(50 + 60 * pulse)));
    drawRoundedRect(cx - 6, cy - 6, cw + 12, ch + 12, 22.f);
    setColor(renderer_, kPanel);
    drawRoundedRect(cx, cy, cw, ch, 16.f);
    setColor(renderer_, col);
    drawRoundedRect(cx, cy, cw, 8.f, 4.f);

    // Textes centrÃĐs : 8 px par caractÃĻre Ã  l'ÃĐchelle 1.
    auto centered = [&](const std::string& s, float scale, float y, Color c) {
        float tw = static_cast<float>(s.size()) * 8.f * scale;
        setColor(renderer_, c);
        drawText(cx + (cw - tw) * 0.5f, y, s, scale);
    };

    centered("VICTOIRE", 2.0f, cy + 34, kTextDim);
    centered(playerName(win) + " a gagne !", 3.0f, cy + 74, col);
    centered("Echap : quitter", 1.5f, cy + ch - 36, kTextDim);
}

void GameScreen::drawMessageToast() {
    if (message_.empty() || game_.isOver()) return;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window_, &winW, &winH);

    float tw = static_cast<float>(message_.size()) * 8.f * 1.5f + 36.f;
    float th = 30.f;
    float tx = (winW - tw) / 2.f;
    float ty = static_cast<float>(winH) - th - 12.f;

    setColor(renderer_, Color{10, 12, 18, 225});
    drawRoundedRect(tx, ty, tw, th, 15.f);
    setColor(renderer_, withAlpha(playerColor(game_.current()), 200));
    drawRoundedRect(tx + 8, ty + th / 2 - 4, 8.f, 8.f, 4.f);
    setColor(renderer_, kText);
    drawText(tx + 24, ty + 9, message_, 1.5f);
}

void GameScreen::render() {
    setColor(renderer_, kBg);
    SDL_RenderClear(renderer_);
    drawGrid();
    drawHighlights();
    drawHud();
    drawMessageToast();
    drawGameOver();
    SDL_RenderPresent(renderer_);
}

}  // namespace ng