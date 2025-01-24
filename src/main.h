#include <cstdint>
#if !defined(MAIN_H)
#include "tile.h"

#define SCREEN_WIDTH 960.
#define SCREEN_HEIGHT 540.
#define PLAYER_HEIGHT 1.80
#define PLAYER_WIDTH (0.70 * PLAYER_HEIGHT)
#define PLAYER_SPEED 5.0f

struct WorldPosition {
  uint32_t abs_tile_x;
  uint32_t abs_tile_y;
  float tile_rel_x;
  float tile_rel_y;
};

struct World {
  TileMap *tile_map;
};

struct GameState {
  WorldPosition player_position;
};

#define MAIN_H
#endif
