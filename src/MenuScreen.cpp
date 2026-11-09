#include "MenuScreen.h"
#include <algorithm>
#include <cmath>

namespace ng {

namespace {
struct Color { Uint8 r, g, b, a; };
constexpr Color kBg      {16, 18, 26, 255};
constexpr Color kPanel   {26, 29, 42, 255};
constexpr Color kPanelHi {40, 45, 66, 255};
constexpr Color kText    {236, 239, 248, 255};
constexpr Color kTextDim {140, 148, 172, 255};
constexpr Color kP1      {64, 140, 255, 255};
constexpr Color kP2      {255, 92, 108, 255};

void setColor(SDL_Renderer* r, Color c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); }
Color withAlpha(Color c, Uint8 a) { return Color{c.r, c.g, c.b, a}; }

constexpr int kWinW = 560, kWinH = 620;
}  // namespace

MenuScreen::~MenuScreen() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
}

bool MenuScreen::init() {
    if (!SDL_CreateWindowAndRenderer("Jeu de carres - Menu", kWinW, kWinH, 0,
                                     &window_, &renderer_)) {
        SDL_Log("CreateWindow: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    layout();
    lastTicks_ = SDL_GetTicks();
    return true;
}

void MenuScreen::layout() {
    const float bw = 400.f, bh = 78.f, gap = 18.f;
    const float x = (kWinW - bw) / 2.f;
    float y = 190.f;
    buttons_[0] = {x, y, bw, bh, "Ordinateur simple", "IA facile",                GameMode::VsComputerEasy};
    y += bh + gap;
    buttons_[1] = {x, y, bw, bh, "Ordinateur expert", "IA qui anticipe",          GameMode::VsComputerHard};
    y += bh + gap;
    buttons_[2] = {x, y, bw, bh, "1 contre 1",        "Deux joueurs, meme ecran", GameMode::LocalOneVsOne};
    y += bh + gap;
    buttons_[3] = {x, y, bw, bh, "Multijoueur",       "Jouer en reseau",          GameMode::Multiplayer};
}

int MenuScreen::buttonAt(float mx, float my) const {
    for (int i = 0; i < kButtonCount; ++i) {
        const Button& b = buttons_[i];
        if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h) return i;
    }
    return -1;
}

void MenuScreen::handleEvent(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_QUIT:
            running_ = false;
            break;
        case SDL_EVENT_MOUSE_MOTION: {
            int h = buttonAt(e.motion.x, e.motion.y);
            hover_ = h;
            if (h >= 0) selected_ = h;
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.button == SDL_BUTTON_LEFT) {
                int h = buttonAt(e.button.x, e.button.y);
                if (h >= 0) { choice_ = buttons_[h].mode; running_ = false; }
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.key == SDLK_ESCAPE) running_ = false;
            else if (e.key.key == SDLK_UP)
                selected_ = (selected_ + kButtonCount - 1) % kButtonCount;
            else if (e.key.key == SDLK_DOWN)
                selected_ = (selected_ + 1) % kButtonCount;
            else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE) {
                choice_ = buttons_[selected_].mode;
                running_ = false;
            }
            break;
        default: break;
    }
}

void MenuScreen::drawRoundedRect(float x, float y, float w, float h, float radius) {
    radius = std::min(radius, std::min(w, h) * 0.5f);
    if (radius < 1.f) {
        SDL_FRect r{x, y, w, h};
        SDL_RenderFillRect(renderer_, &r);
        return;
    }
    SDL_FRect mid{x, y + radius, w, h - 2 * radius};
    SDL_RenderFillRect(renderer_, &mid);
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

void MenuScreen::drawText(float x, float y, const std::string& s, float scale) {
    SDL_SetRenderScale(renderer_, scale, scale);
    SDL_RenderDebugText(renderer_, x / scale, y / scale, s.c_str());
    SDL_SetRenderScale(renderer_, 1.f, 1.f);
}

void MenuScreen::render() {
    setColor(renderer_, kBg);
    SDL_RenderClear(renderer_);

    auto centered = [&](const std::string& s, float scale, float y, Color c) {
        float tw = static_cast<float>(s.size()) * 8.f * scale;
        setColor(renderer_, c);
        drawText((kWinW - tw) * 0.5f, y, s, scale);
    };

    // Titre : deux pastilles aux couleurs des joueurs + nom du jeu.
    float pulse = 0.7f + 0.3f * std::sin(time_ * 3.f);
    setColor(renderer_, withAlpha(kP1, static_cast<Uint8>(255 * pulse)));
    drawRoundedRect(kWinW / 2.f - 34, 52, 28, 28, 7.f);
    setColor(renderer_, withAlpha(kP2, static_cast<Uint8>(255 * pulse)));
    drawRoundedRect(kWinW / 2.f + 6, 52, 28, 28, 7.f);

    centered("JEU DE CARRES", 3.0f, 100, kText);
    centered("Choisissez un mode de jeu", 1.5f, 148, kTextDim);

    for (int i = 0; i < kButtonCount; ++i) {
        const Button& b = buttons_[i];
        const bool sel = (i == selected_);
        // Bleu pour les modes contre l'IA, rouge pour les modes a deux.
        const Color accent = (i >= 2) ? kP2 : kP1;

        setColor(renderer_, sel ? kPanelHi : kPanel);
        drawRoundedRect(b.x, b.y, b.w, b.h, 14.f);

        setColor(renderer_, sel ? accent : withAlpha(accent, 110));
        drawRoundedRect(b.x, b.y, 7.f, b.h, 3.f);
        if (sel) {
            setColor(renderer_, withAlpha(accent, 200));
            SDL_FRect o{b.x, b.y, b.w, b.h};
            SDL_RenderRect(renderer_, &o);
        }

        setColor(renderer_, kText);
        drawText(b.x + 28, b.y + 18, b.label, 2.0f);
        setColor(renderer_, kTextDim);
        drawText(b.x + 28, b.y + 48, b.sub, 1.5f);
    }

    centered("Haut/Bas : choisir   Entree : valider   Echap : quitter",
             1.0f, kWinH - 28, kTextDim);

    SDL_RenderPresent(renderer_);
}

GameMode MenuScreen::run() {
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);
        Uint64 now = SDL_GetTicks();
        time_ += static_cast<float>(now - lastTicks_) / 1000.f;
        lastTicks_ = now;
        render();
        SDL_Delay(16);
    }
    return choice_;
}

}  // namespace ng