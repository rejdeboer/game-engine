

#define internal static
#define f32 float
#define f64 double
#define i32 int
#define u32 unsigned int

#define SCREEN_WIDTH 960.
#define SCREEN_HEIGHT 540.
#define PLAYER_HEIGHT 50.
#define PLAYER_WIDTH 30.
#define PLAYER_SPEED ((double)SCREEN_HEIGHT / 200.)

struct TileMap {
  u32 n_rows;
  u32 n_columns;
  f32 start_x;
  f32 start_y;
  f32 tile_width;
  f32 tile_height;
  u32 *tiles;
};

struct World {
  u32 n_tile_map_x;
  u32 n_tile_map_y;
  TileMap *tile_maps;
};

struct GameState {
  f32 player_x;
  f32 player_y;
};
