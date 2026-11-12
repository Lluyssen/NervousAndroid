#include "GameScreen_Internal.h"

namespace ng {

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
    if (!canAct()) return;

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

void GameScreen::render() {
    setColor(renderer_, kBg);
    SDL_RenderClear(renderer_);
    drawGrid();
    drawHighlights();
    drawHud();
    drawMessageToast();
    drawGameOver();
    if (paused_) drawPauseMenu();          // AJOUT : menu pause par-dessus
    SDL_RenderPresent(renderer_);
}

}  // namespace ng
