#include "game.h"

int main(int argc, char *args[]) {
    Game game;
    game.init();
    game.run();
    game.deinit();
}
