

#define internal static
#define f32 float
#define f64 double
#define i32 int
#define u32 unsigned int

#define SCREEN_WIDTH 960.
#define SCREEN_HEIGHT 540.
#define PLAYER_HEIGHT 1.80
#define PLAYER_WIDTH (0.75 * PLAYER_HEIGHT)
#define PLAYER_SPEED 5.0f

struct TileMap {
  u32 *tiles;
};

struct WorldPosition {
  u32 tile_map_x;
  u32 tile_map_y;
  i32 tile_x;
  i32 tile_y;
  f32 tile_rel_x;
  f32 tile_rel_y;
};

struct World {
  f32 tile_side_in_meters;
  i32 tile_side_in_pixels;
  f32 meters_to_pixels;

  u32 n_tile_map_x;
  u32 n_tile_map_y;

  u32 n_rows;
  u32 n_columns;
  f32 start_x;
  f32 start_y;

  TileMap *tile_maps;
};

struct GameState {
  WorldPosition player_position;
};
