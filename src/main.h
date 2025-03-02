#if !defined(MAIN_H)
#include "memory.h"
#include "tile.h"
#include <SDL3/SDL.h>

// TODO: This value is completely arbitrary
#define GAME_MEMORY 1024 * 1024 * 64

#define SCREEN_WIDTH 960.
#define SCREEN_HEIGHT 540.
#define PLAYER_HEIGHT 1.80
#define PLAYER_WIDTH (0.70 * PLAYER_HEIGHT)
#define PLAYER_SPEED 5.0f

struct EngineStats {
    Uint64 frameTime;
    float fps;
    int triangleCount;
    int drawcallCount;
    Uint64 meshDrawTime;
};

struct GameState {
    Arena arena;
    WorldPosition player_position;
    EngineStats stats;
};

#define MAIN_H
#endif
