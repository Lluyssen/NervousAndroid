#include "GameScreen.h"
#include "Rules.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace ng {

namespace {
struct Rgba { Uint8 r, g, b, a; };

// --- Palette ---
constexpr Rgba kBg       {16, 18, 26, 255};
constexpr Rgba kPanel    {26, 29, 42, 255};
constexpr Rgba kGridCell {34, 38, 54, 255};
constexpr Rgba kText     {236, 239, 248, 255};
constexpr Rgba kTextDim  {140, 148, 172, 255};
constexpr Rgba kP1       {64, 140, 255, 255};
constexpr Rgba kP1Dark   {30, 80, 180, 255};
constexpr Rgba kP2       {255, 92, 108, 255};
constexpr Rgba kP2Dark   {180, 44, 64, 255};
constexpr Rgba kHint     {255, 236, 130, 95};
constexpr Rgba kGood     {80, 220, 140, 255};
constexpr Rgba kCard     {28, 32, 48, 255};
constexpr Rgba kIllegal  {255, 80, 80, 140};

// --- Mise en page (HUD) ---
constexpr float kHudHeight   = 90.f;
constexpr float kCardY       = 8.f;
constexpr float kCardH       = 66.f;
constexpr float kMargin      = 12.f;
constexpr float kCardGap     = 12.f;
constexpr float kStatusY     = 78.f;
constexpr float kStatusH     = 28.f;

// Budget de réflexion du MCTS (ms) et délai visuel du niveau Facile (ms).
constexpr int    kAiThinkMs   = 700;
constexpr Uint64 kEasyDelayMs = 350;

void setColor(SDL_Renderer* r, Rgba c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
Rgba withAlpha(Rgba c, Uint8 a) { return Rgba{c.r, c.g, c.b, a}; }
Rgba playerColor(Player p) { return p == Player::One ? kP1 : kP2; }
}  // namespace

GameScreen::GameScreen(int width, int height) : game_(width, height) {}

GameScreen::~GameScreen() {
    if (aiThread_.joinable()) aiThread_.join();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
}

bool GameScreen::init() {
    const Board& b = game_.board();
    cellSize_ = std::max(6, std::min(28, 900 / std::max(b.width(), b.height())));

    const int w = std::max(b.width() * cellSize_, 560);
    const int h = b.height() * cellSize_ + static_cast<int>(kHudHeight);

    if (!SDL_CreateWindowAndRenderer("Jeu de carres", w, h, 0, &window_, &renderer_)) {
        SDL_Log("CreateWindow: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    maxHp_ = std::max(1, std::max(game_.hp(Player::One), game_.hp(Player::Two)));
    prevHp_[0] = game_.hp(Player::One);
    prevHp_[1] = game_.hp(Player::Two);
    lastTicks_ = SDL_GetTicks();

    // Purge les événements hérités du menu : évite un double clic fantôme.
    // Sans cela, un clic de validation du menu serait compté sur le plateau.
    SDL_RaiseWindow(window_);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    startTicks_ = SDL_GetTicks();

    rebuildCache();
    setMessage("J1 : placez votre base.");
    return true;
}

void GameScreen::setMode(GameMode m) {
    mode_ = m;
    aiLevel_ = (m == GameMode::VsComputerHard) ? AiLevel::Hard : AiLevel::Easy;
}

// ---------------------------------------------------------------------------
//  Helpers d'état
// ---------------------------------------------------------------------------

// Joueur qui doit agir maintenant (tient compte du déplacement de base forcé).
Player GameScreen::currentActor() const {
    return game_.phase() == Phase::RelocateBase ? game_.mustRelocatePlayer()
                                                : game_.current();
}

// Vrai si c'est à l'ordinateur de jouer. Point unique de vérité.
bool GameScreen::isComputerTurn() const {
    return vsComputer() && !game_.isOver() && currentActor() == Player::Two;
}

// Un seul point de passage après toute modification de la partie.
void GameScreen::afterStateChange(Result r) {
    setMessage(describe(r));
    rebuildCache();
}

// ---------------------------------------------------------------------------
//  Intelligence artificielle
// ---------------------------------------------------------------------------

// Lance la réflexion MCTS dans un thread, sur une COPIE de la partie.
void GameScreen::startAiThinking() {
    if (aiBusy_.load()) return;
    if (aiThread_.joinable()) aiThread_.join();
    aiBusy_ = true;
    aiReady_ = false;

    Game snapshot = game_;
    aiThread_ = std::thread([this, snapshot]() {
        Move m = mcts_.chooseMove(snapshot, kAiThinkMs);
        aiMove_ = m;             // écrit avant la publication de aiReady_
        aiReady_.store(true);    // release : rend aiMove_ visible au thread principal
    });
}

// Applique le coup calculé par le MCTS (thread principal).
void GameScreen::applyAiMove() {
    if (aiThread_.joinable()) aiThread_.join();
    aiBusy_ = false;
    aiReady_ = false;

    Result r = Result::WrongPhase;
    switch (aiMove_.kind) {
        case Move::Kind::Square:   r = game_.placeSquare(aiMove_.pos);  break;
        case Move::Kind::EndTurn:  r = game_.endTurn();                 break;
        case Move::Kind::Base:     r = game_.placeBase(aiMove_.pos);    break;
        case Move::Kind::Relocate: r = game_.relocateBase(aiMove_.pos); break;
    }

    if (r == Result::Ok) {
        afterStateChange(r);
    } else {
        // Le MCTS a proposé un coup refusé : un seul repli, sans boucle.
        fallbackAiMove();
    }
}

// Coup de repli aléatoire légal. Garantit la progression de la partie.
void GameScreen::fallbackAiMove() {
    Result r = Result::WrongPhase;

    switch (game_.phase()) {
        case Phase::PlaceBaseP1:
        case Phase::PlaceBaseP2: {
            auto cells = game_.legalInitialBasePlacements();
            if (!cells.empty()) {
                std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                r = game_.placeBase(cells[d(rng_)]);
            }
            break;
        }
        case Phase::RelocateBase: {
            auto cells = game_.legalBaseRelocations();
            if (!cells.empty()) {
                std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                r = game_.relocateBase(cells[d(rng_)]);
            }
            break;
        }
        case Phase::PlaySquares: {
            auto cells = game_.legalSquarePlacements();
            // Aucune pose possible : on termine le tour (sinon blocage).
            if (cells.empty()) {
                r = game_.endTurn();
                break;
            }
            std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
            r = game_.placeSquare(cells[d(rng_)]);
            break;
        }
        default:
            break;
    }

    // Garde-fou : tour en cours, rien joué, au moins une pose faite -> fin de tour.
    if (r != Result::Ok && game_.phase() == Phase::PlaySquares &&
        game_.placedThisTurn() >= 1) {
        r = game_.endTurn();
    }

    afterStateChange(r);
}

void GameScreen::playComputerTurn() {
    if (aiLevel_ == AiLevel::Easy) {
        // IA facile : coups légaux aléatoires, avec un délai pour qu'on les voie.
        const Uint64 now = SDL_GetTicks();
        if (now - lastEasyMoveTicks_ < kEasyDelayMs) return;
        lastEasyMoveTicks_ = now;
        fallbackAiMove();
        return;
    }

    // IA experte : MCTS asynchrone.
    if (aiReady_.load())      applyAiMove();
    else if (!aiBusy_.load()) startAiThinking();
}

// ---------------------------------------------------------------------------
//  Boucle principale et événements
// ---------------------------------------------------------------------------

void GameScreen::run() {
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);

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
            if (e.button.button == SDL_BUTTON_LEFT) handleClick(e.button.x, e.button.y);
            break;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.key == SDLK_ESCAPE) running_ = false;
            if ((e.key.key == SDLK_SPACE || e.key.key == SDLK_RETURN) && !isComputerTurn()) {
                afterStateChange(game_.endTurn());
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

// Recalcule les cases jouables. Appelé uniquement après un changement d'état.
void GameScreen::rebuildCache() {
    const Board& b = game_.board();
    const int W = b.width(), H = b.height();

    std::vector<Pos> cells;
    legalTlMask_.assign(static_cast<size_t>(W) * H, 0);
    hintMask_.assign(static_cast<size_t>(W) * H, 0);

    const Phase ph = game_.phase();
    const bool isBase = (ph == Phase::PlaceBaseP1 || ph == Phase::PlaceBaseP2 ||
                         ph == Phase::RelocateBase);

    switch (ph) {
        case Phase::PlaceBaseP1:
        case Phase::PlaceBaseP2:  cells = game_.legalInitialBasePlacements(); break;
        case Phase::RelocateBase: cells = game_.legalBaseRelocations();       break;
        case Phase::PlaySquares:  cells = game_.legalSquarePlacements();      break;
        default: break;
    }

    for (const Pos& p : cells) {
        if (p.x < 0 || p.y < 0 || p.x >= W || p.y >= H) continue;
        legalTlMask_[static_cast<size_t>(p.y) * W + p.x] = 1;

        if (isBase) {
            // Une base couvre 2x2 : on marque les 4 cases.
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const int cx = p.x + dx, cy = p.y + dy;
                    if (cx < W && cy < H)
                        hintMask_[static_cast<size_t>(cy) * W + cx] = 1;
                }
        } else {
            hintMask_[static_cast<size_t>(p.y) * W + p.x] = 1;
        }
    }
}

// Cherche le coin haut-gauche légal le plus proche d'un clic (2x2 de la base).
// Ordre explicite : la case cliquée, puis décalée vers le haut/gauche.
bool GameScreen::findLegalTl(Pos click, Pos* out) const {
    const int W = game_.board().width(), H = game_.board().height();
    static constexpr int kOffsets[2] = {0, -1};
    for (int dy : kOffsets)
        for (int dx : kOffsets) {
            const int tx = click.x + dx, ty = click.y + dy;
            if (tx < 0 || ty < 0 || tx >= W || ty >= H) continue;
            if (legalTlMask_[static_cast<size_t>(ty) * W + tx]) {
                *out = Pos{tx, ty};
                return true;
            }
        }
    return false;
}

void GameScreen::handleClick(float mx, float my) {
    if (SDL_GetTicks() - startTicks_ < 250) return;
    if (game_.isOver() || isComputerTurn()) return;

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
            Pos tl;
            r = game_.relocateBase(findLegalTl(p, &tl) ? tl : p);
            break;
        }
        case Phase::GameOver:
            return;
    }
    afterStateChange(r);
}

// ---------------------------------------------------------------------------
//  Primitives de dessin
// ---------------------------------------------------------------------------

void GameScreen::drawRoundedRect(float x, float y, float w, float h, float radius) {
    radius = std::min(radius, std::min(w, h) * 0.5f);
    if (radius < 1.f) {
        SDL_FRect r{x, y, w, h};
        SDL_RenderFillRect(renderer_, &r);
        return;
    }
    SDL_FRect mid{x, y + radius, w, h - 2 * radius};
    SDL_RenderFillRect(renderer_, &mid);

    const int rr = static_cast<int>(radius);
    for (int i = 0; i < rr; ++i) {
        const float dy = radius - i - 0.5f;
        const float dx = radius - std::sqrt(std::max(0.f, radius * radius - dy * dy));
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
    if (vsComputer() && pl == Player::Two) return "Ordinateur";
    return pl == Player::One ? "Joueur 1" : "Joueur 2";
}

void GameScreen::updateAnimations() {
    const Uint64 now = SDL_GetTicks();
    const float dt = static_cast<float>(now - lastTicks_) / 1000.f;
    lastTicks_ = now;
    time_ += dt;

    for (int i = 0; i < 2; ++i) {
        const Player pl = (i == 0) ? Player::One : Player::Two;
        const int hp = game_.hp(pl);
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

    // Un seul parcours : fond de case puis liseret sur les bases.
    for (int y = 0; y < b.height(); ++y) {
        for (int x = 0; x < b.width(); ++x) {
            const float fx = static_cast<float>(x * cellSize_);
            const float fy = static_cast<float>(y * cellSize_) + kHudHeight;
            const float sz = static_cast<float>(cellSize_) - gap;

            const Cell c = b.get(Pos{x, y});
            if (c == squareOf(Player::One))      setColor(renderer_, kP1);
            else if (c == squareOf(Player::Two)) setColor(renderer_, kP2);
            else if (c == baseOf(Player::One))   setColor(renderer_, kP1Dark);
            else if (c == baseOf(Player::Two))   setColor(renderer_, kP2Dark);
            else                                 setColor(renderer_, kGridCell);
            drawRoundedRect(fx, fy, sz, sz, rad);

            const bool b1 = (c == baseOf(Player::One));
            const bool b2 = (c == baseOf(Player::Two));
            if (b1 || b2) {
                setColor(renderer_, withAlpha(b1 ? kP1 : kP2, 150));
                const float m = std::max(1.f, cellSize_ * 0.18f);
                SDL_FRect r{fx + m, fy + m, sz - 2 * m, sz - 2 * m};
                SDL_RenderFillRect(renderer_, &r);
            }
        }
    }
}

void GameScreen::drawHighlights() {
    if (game_.phase() == Phase::GameOver) return;
    if (isComputerTurn()) return;

    const Board& b = game_.board();
    const int W = b.width(), H = b.height();

    const float pulse = 0.65f + 0.35f * std::sin(time_ * 4.f);
    setColor(renderer_, withAlpha(kHint, static_cast<Uint8>(kHint.a * pulse + 40)));

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            if (!hintMask_[static_cast<size_t>(y) * W + x]) continue;
            const float m = cellSize_ * 0.30f;
            drawRoundedRect(x * (float)cellSize_ + m,
                            y * (float)cellSize_ + kHudHeight + m,
                            cellSize_ - 2 * m, cellSize_ - 2 * m,
                            (cellSize_ - 2 * m) * 0.5f);
        }

    // Aperçu sous la souris.
    const Phase ph = game_.phase();
    const bool isBase = (ph == Phase::PlaceBaseP1 || ph == Phase::PlaceBaseP2 ||
                         ph == Phase::RelocateBase);
    const bool isSquare = (ph == Phase::PlaySquares);
    if (!isBase && !isSquare) return;

    float mx, my;
    SDL_GetMouseState(&mx, &my);
    Pos hover;
    if (!pixelToCell(mx, my, &hover)) return;

    const Rgba pc = playerColor(currentActor());

    if (isBase) {
        Pos tl;
        if (findLegalTl(hover, &tl)) {
            setColor(renderer_, withAlpha(pc, 150));
            drawRoundedRect(tl.x * (float)cellSize_,
                            tl.y * (float)cellSize_ + kHudHeight,
                            cellSize_ * 2 - 1.f, cellSize_ * 2 - 1.f, 4.f);
            setColor(renderer_, Rgba{255, 255, 255, 120});
            SDL_FRect o{tl.x * (float)cellSize_,
                        tl.y * (float)cellSize_ + kHudHeight,
                        cellSize_ * 2 - 1.f, cellSize_ * 2 - 1.f};
            SDL_RenderRect(renderer_, &o);
        } else {
            setColor(renderer_, kIllegal);
            SDL_FRect r{hover.x * (float)cellSize_,
                        hover.y * (float)cellSize_ + kHudHeight,
                        cellSize_ - 1.f, cellSize_ - 1.f};
            SDL_RenderFillRect(renderer_, &r);
        }
    } else {
        const bool legal = hintMask_[static_cast<size_t>(hover.y) * W + hover.x] != 0;
        setColor(renderer_, legal ? withAlpha(pc, 160) : kIllegal);
        SDL_FRect r{hover.x * (float)cellSize_,
                    hover.y * (float)cellSize_ + kHudHeight,
                    cellSize_ - 1.f, cellSize_ - 1.f};
        SDL_RenderFillRect(renderer_, &r);
    }
}

// ---------------------------------------------------------------------------
//  HUD
// ---------------------------------------------------------------------------

void GameScreen::drawPlayerCard(Player pl, float x, float y, float w, float h) {
    const int idx = (pl == Player::One) ? 0 : 1;
    const bool active = (currentActor() == pl) && !game_.isOver();
    const Rgba pc = playerColor(pl);

    setColor(renderer_, kCard);
    drawRoundedRect(x, y, w, h, 12.f);

    if (active) {
        const float pulse = 0.6f + 0.4f * std::sin(time_ * 4.f);
        setColor(renderer_, withAlpha(pc, static_cast<Uint8>(120 + 100 * pulse)));
        SDL_FRect o{x, y, w, h};
        SDL_RenderRect(renderer_, &o);
    }

    setColor(renderer_, pc);
    drawRoundedRect(x + 14, y + (h - 14.f) * 0.5f, 14.f, 14.f, 7.f);
    setColor(renderer_, kText);
    drawText(x + 38, y + (h - 16.f) * 0.5f, playerName(pl), 2.0f);

    // PV : un carré par point de vie.
    const int hp = game_.hp(pl);
    const int total = std::max(1, maxHp_);
    const float gap = 6.f;
    const float box = std::min(20.f, h - 20.f);
    const float rowW = total * box + (total - 1) * gap;
    const float counterW = 50.f;
    const float bx = x + w - 14.f - counterW - rowW;
    const float by = y + (h - box) * 0.5f;

    for (int i = 0; i < total; ++i) {
        const float cx = bx + i * (box + gap);
        if (i < hp) {
            setColor(renderer_, pc);
            drawRoundedRect(cx, by, box, box, 5.f);
            setColor(renderer_, Rgba{255, 255, 255, 60});
            drawRoundedRect(cx + 3, by + 3, box - 6, (box - 6) * 0.45f, 3.f);
        } else {
            setColor(renderer_, Rgba{255, 255, 255, 18});
            drawRoundedRect(cx, by, box, box, 5.f);
            setColor(renderer_, Rgba{255, 255, 255, 45});
            SDL_FRect o{cx, by, box, box};
            SDL_RenderRect(renderer_, &o);
        }
    }

    // Flash rouge sur le PV qui vient de disparaître.
    if (lostFlash_[idx] > 0.f && hp >= 0 && hp < total) {
        const float cx = bx + hp * (box + gap);
        setColor(renderer_, Rgba{255, 70, 70,
                 static_cast<Uint8>(200 * lostFlash_[idx])});
        drawRoundedRect(cx, by, box, box, 5.f);
    }

    const std::string t = std::to_string(hp) + "/" + std::to_string(total);
    setColor(renderer_, kTextDim);
    drawText(x + w - 14.f - counterW + 6.f, y + (h - 12.f) * 0.5f, t, 1.5f);
}

void GameScreen::drawStatusBar(float w) {
    const float margin = kMargin;
    const Player cur = currentActor();
    const Rgba accent = game_.isOver() ? kGood : playerColor(cur);

    std::string phase;
    if (game_.isOver()) {
        phase = "Partie terminee";
    } else {
        const bool computerTurn = isComputerTurn();
        switch (game_.phase()) {
            case Phase::PlaceBaseP1:
            case Phase::PlaceBaseP2:
                phase = computerTurn ? "Place sa base" : "Placez votre base";
                break;
            case Phase::PlaySquares:
                phase = computerTurn ? "Reflechit..."
                                     : "Posez vos carres  -  Espace : fin de tour";
                break;
            case Phase::RelocateBase:
                phase = computerTurn ? "Deplace sa base" : "Deplacez votre base";
                break;
            default: break;
        }
        phase = "Tour de " + playerName(cur) + "  |  " + phase;
    }

    const float textScale = 1.5f;
    const float textW = static_cast<float>(phase.size()) * 8.f * textScale;
    const float textH = 8.f * textScale;
    const float padX = 24.f;
    const float barW = std::min(w - 2 * margin, textW + 2 * padX);
    const float barX = (w - barW) * 0.5f;

    setColor(renderer_, kPanel);
    drawRoundedRect(barX, kStatusY, barW, kStatusH, 10.f);
    setColor(renderer_, accent);
    drawRoundedRect(barX, kStatusY, 6.f, kStatusH, 3.f);

    setColor(renderer_, kText);
    drawText(barX + (barW - textW) * 0.5f,
             kStatusY + (kStatusH - textH) * 0.5f, phase, textScale);
}

void GameScreen::drawHud() {
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window_, &winW, &winH);
    const float w = static_cast<float>(winW);

    setColor(renderer_, kBg);
    SDL_FRect bg{0, 0, w, kHudHeight};
    SDL_RenderFillRect(renderer_, &bg);

    const float cardW = (w - 2 * kMargin - kCardGap) / 2.f;
    drawPlayerCard(Player::One, kMargin, kCardY, cardW, kCardH);
    drawPlayerCard(Player::Two, kMargin + cardW + kCardGap, kCardY, cardW, kCardH);

    drawStatusBar(w);
}

void GameScreen::drawGameOver() {
    if (!game_.isOver()) return;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window_, &winW, &winH);
    const float w = static_cast<float>(winW), h = static_cast<float>(winH);

    setColor(renderer_, Rgba{8, 9, 14, 200});
    SDL_FRect veil{0, 0, w, h};
    SDL_RenderFillRect(renderer_, &veil);

    if (!game_.winner().has_value()) return;
    const Player win = *game_.winner();
    const Rgba col = playerColor(win);

    const float cw = std::min(w - 40.f, 460.f), ch = 190.f;
    const float cx = (w - cw) / 2.f, cy = (h - ch) / 2.f;

    const float pulse = 0.6f + 0.4f * std::sin(time_ * 3.f);
    setColor(renderer_, withAlpha(col, static_cast<Uint8>(50 + 60 * pulse)));
    drawRoundedRect(cx - 6, cy - 6, cw + 12, ch + 12, 22.f);
    setColor(renderer_, kPanel);
    drawRoundedRect(cx, cy, cw, ch, 16.f);
    setColor(renderer_, col);
    drawRoundedRect(cx, cy, cw, 8.f, 4.f);

    auto centered = [&](const std::string& s, float scale, float y, Rgba c) {
        const float tw = static_cast<float>(s.size()) * 8.f * scale;
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

    const float tw = static_cast<float>(message_.size()) * 8.f * 1.5f + 36.f;
    const float th = 30.f;
    const float tx = (winW - tw) / 2.f;
    const float ty = static_cast<float>(winH) - th - 12.f;

    setColor(renderer_, Rgba{10, 12, 18, 225});
    drawRoundedRect(tx, ty, tw, th, 15.f);
    setColor(renderer_, withAlpha(playerColor(currentActor()), 200));
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