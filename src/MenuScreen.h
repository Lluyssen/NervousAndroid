#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace ng {

enum class GameMode { None, VsComputerEasy, VsComputerHard, LocalOneVsOne, Multiplayer, LoadSave };

class MenuScreen {
public:
    MenuScreen() = default;
    ~MenuScreen();

    bool init();
    GameMode run();

private:
    static constexpr int kButtonCount = 4;   // taille max du tableau

    struct Button {
        float x = 0, y = 0, w = 0, h = 0;
        std::string label;
        std::string sub;
        GameMode mode = GameMode::None;
        bool opensSubmenu = false;           // vrai pour le bouton "Ordinateur"
    };

    enum class Page { Main, Computer };
    Page page_        = Page::Main;
    int  buttonCount_ = 3;                   // nombre de boutons de la page courante

    void showPage(Page p);                   // remplace layout()
    void activate(int i);                    // clic / Entree sur un bouton
    int  buttonAt(float mx, float my) const;
    void handleEvent(const SDL_Event& e);
    void render();
    void drawRoundedRect(float x, float y, float w, float h, float radius);
    void drawText(float x, float y, const std::string& s, float scale);

    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    Button   buttons_[kButtonCount];
    int      selected_ = 0;
    int      hover_    = -1;
    bool     running_  = true;
    GameMode choice_   = GameMode::None;

    Uint64 lastTicks_ = 0;
    float  time_      = 0.f;
};

}  // namespace ng