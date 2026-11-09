#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace ng {

enum class GameMode {
    None,
    VsComputerEasy,
    VsComputerHard,
    LocalOneVsOne,
    Multiplayer
};

class MenuScreen {
public:
    MenuScreen() = default;
    ~MenuScreen();

    bool init();
    GameMode run();

private:
    static constexpr int kButtonCount = 4;

    struct Button {
        float x, y, w, h;
        std::string label;
        std::string sub;
        GameMode mode;
    };

    void layout();
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