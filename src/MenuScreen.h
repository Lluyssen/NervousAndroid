#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace ng {

enum class GameMode { None, VsComputer, LocalOneVsOne, Multiplayer };

class MenuScreen {
public:
    MenuScreen() = default;
    ~MenuScreen();

    bool init();
    // Boucle du menu. Retourne le mode choisi, ou None si la fenÃŠtre est fermÃĐe.
    GameMode run();

private:
    struct Button { float x, y, w, h; std::string label; std::string sub; GameMode mode; };

    void handleEvent(const SDL_Event& e);
    void render();
    void layout();
    int  buttonAt(float mx, float my) const;
    void drawRoundedRect(float x, float y, float w, float h, float radius);
    void drawText(float x, float y, const std::string& s, float scale);

    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Button  buttons_[3];
    int     hover_    = -1;
    int     selected_ = 0;      // sÃĐlection clavier
    float   time_     = 0.f;
    Uint64  lastTicks_ = 0;
    bool    running_  = true;
    GameMode choice_  = GameMode::None;
};

}  // namespace ng