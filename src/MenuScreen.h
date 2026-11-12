#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace ng {

enum class GameMode { None, VsComputerEasy, VsComputerHard, LocalOneVsOne,
                      Host, Join, LoadSave };

class MenuScreen {
public:
    MenuScreen() = default;
    ~MenuScreen();

    bool init();
    GameMode run();
    bool wantsLoad() const { return loadRequested_; }

private:
    static constexpr int kButtonCount = 4;

    struct Button {
        float x = 0, y = 0, w = 0, h = 0;
        std::string label;
        std::string sub;
        GameMode mode = GameMode::None;
        int  goPage = -1;        // page a ouvrir, -1 = bouton final
        bool isBack = false;
        bool isLoad = false;
    };

    enum class Page { Main, Offline, Computer, Online, NewOrLoad };
    Page page_        = Page::Main;
    int  buttonCount_ = 0;

    void showPage(Page p);
    void activate(int i);
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

    GameMode pendingMode_   = GameMode::None;   // LocalOneVsOne ou Host
    bool     loadRequested_ = false;

    Uint64 lastTicks_ = 0;
    float  time_      = 0.f;
};

}  // namespace ng