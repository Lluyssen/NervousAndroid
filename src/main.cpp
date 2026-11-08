#include "GameScreen.h"

int main(int, char**) {
    ng::GameScreen screen(30, 30);
    if (!screen.init()) return 1;
    screen.run();
    return 0;
}