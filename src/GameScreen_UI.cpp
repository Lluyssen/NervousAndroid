#include "GameScreen_Internal.h"
#include <cstring>

namespace ng {

std::string GameScreen::playerName(Player pl) const {
    if (vsComputer() && pl == Player::Two) return "Ordinateur";
    std::string n = (pl == Player::One) ? "Joueur 1" : "Joueur 2";
    if (online() && pl == myPlayer_) n += " (vous)";
    return n;
}

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

    // Pastille + nom (échelle 1.5 : tient dans la carte, même avec "(vous)").
    setColor(renderer_, pc);
    drawRoundedRect(x + 14, y + (h - 14.f) * 0.5f, 14.f, 14.f, 7.f);
    setColor(renderer_, kText);
    drawText(x + 38, y + (h - 12.f) * 0.5f, playerName(pl), 1.5f);

    // PV : un carré par point de vie, taille adaptée à la place restante.
    const int hp = game_.hp(pl);
    const int total = std::max(1, maxHp_);
    const float counterW = 50.f;
    const float gap = 4.f;

    // Zone PV = 55 % de la carte, a droite, a cote du compteur.
    const float zoneW = w * 0.55f - counterW - 14.f;
    float box = std::min(20.f, h - 20.f);
    const float needed = total * box + (total - 1) * gap;
    if (needed > zoneW)
        box = std::max(6.f, (zoneW - (total - 1) * gap) / total);

    const float rowW = total * box + (total - 1) * gap;
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
    if (online() && connectionLost_) {
        phase = "Connexion perdue";
    } else if (online() && replaying_) {
        phase = "Resynchronisation...";
    } else if (online() && !gameStarted_) {
        phase = "En attente d'un adversaire...";
    } else if (online() && opponentLeft_) {
        phase = "Adversaire deconnecte";
    } else if (game_.isOver()) {
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
        if (online())
            phase = (cur == myPlayer_ ? "A vous de jouer" : "Tour de l'adversaire")
                    + std::string("  |  ") + phase;
        else
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

// ---------------------------------------------------------------------------
// Menu pause
// ---------------------------------------------------------------------------

GameScreen::PauseButton GameScreen::pauseButton(int i) const {
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window_, &winW, &winH);
    const float bw = std::min(300.f, winW - 40.f), bh = 56.f, gap = 16.f;
    const float x = (winW - bw) / 2.f;
    const float y0 = winH / 2.f - bh - gap / 2.f;
    static const char* labels[2] = {"Sauvegarder", "Retour Menu"};
    return {x, y0 + i * (bh + gap), bw, bh, labels[i]};
}

int GameScreen::pauseButtonAt(float mx, float my) const {
    for (int i = 0; i < 2; ++i) {
        if (i == 0 && online()) continue;   // pas de sauvegarde en reseau
        const PauseButton b = pauseButton(i);
        if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h)
            return i;
    }
    return -1;
}

void GameScreen::drawPauseMenu() {
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window_, &winW, &winH);
    const float w = static_cast<float>(winW), h = static_cast<float>(winH);

    setColor(renderer_, Rgba{8, 9, 14, 200});
    SDL_FRect veil{0, 0, w, h};
    SDL_RenderFillRect(renderer_, &veil);

    auto centered = [&](const std::string& s, float scale, float y, Rgba c) {
        const float tw = static_cast<float>(s.size()) * 8.f * scale;
        setColor(renderer_, c);
        drawText((w - tw) * 0.5f, y, s, scale);
    };
    centered("PAUSE", 3.0f, h / 2.f - 140.f, kText);

    for (int i = 0; i < 2; ++i) {
        const PauseButton b = pauseButton(i);
        const bool disabled = (i == 0 && online());
        const bool hot = (i == pauseHover_) && !disabled;

        setColor(renderer_, hot ? Rgba{40, 45, 66, 255} : kPanel);
        drawRoundedRect(b.x, b.y, b.w, b.h, 14.f);

        const Rgba accent = (i == 0) ? kGood : kP2;
        setColor(renderer_, disabled ? kTextDim
                                     : withAlpha(accent, hot ? 255 : 130));
        drawRoundedRect(b.x, b.y, 7.f, b.h, 3.f);

        const float scale = 2.0f;
        const float tw = std::strlen(b.label) * 8.f * scale;
        setColor(renderer_, disabled ? kTextDim : kText);
        drawText(b.x + (b.w - tw) * 0.5f + 3.f,
                 b.y + (b.h - 8.f * scale) * 0.5f, b.label, scale);
    }

    if (SDL_GetTicks() < savedMsgUntil_)
        centered("Partie sauvegardee", 1.5f,
                 pauseButton(1).y + pauseButton(1).h + 24.f, kGood);

    centered("Echap : reprendre", 1.0f, h - 28.f, kTextDim);
}

void GameScreen::handlePauseClick(float mx, float my) {
    const int i = pauseButtonAt(mx, my);
    if (i == 0) {
       if (game_.saveToFile("save.txt", static_cast<int>(mode_)))
            savedMsgUntil_ = SDL_GetTicks() + 2000;
        else
            setMessage("Echec de la sauvegarde");
    } else if (i == 1) {
        wantsMenu_ = true;
    }
}

}  // namespace ng