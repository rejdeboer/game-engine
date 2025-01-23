#define f32 float
#define f64 double
#define i32 int
#define u32 unsigned int

#define SCREEN_WIDTH 960.
#define SCREEN_HEIGHT 540.
#define PLAYER_HEIGHT 1.80
#define PLAYER_WIDTH (0.75 * PLAYER_HEIGHT)
#define PLAYER_SPEED 5.0f

struct TileChunk {
  u32 *tiles;
};

struct WorldPosition {
  u32 abs_tile_x;
  u32 abs_tile_y;
  f32 tile_rel_x;
  f32 tile_rel_y;
};

struct TileChunkPosition {
  u32 chunk_x;
  u32 chunk_y;
  u32 tile_x;
  u32 tile_y;
};

struct World {
  f32 tile_side_in_meters;
  i32 tile_side_in_pixels;
  f32 meters_to_pixels;

  // NOTE: We use 256x256 tile chunks
  u32 chunk_shift;
  u32 chunk_mask;
  u32 chunk_dim;

  u32 n_tile_chunk_x;
  u32 n_tile_chunk_y;

  TileChunk *tile_chunks;
};

struct GameState {
  WorldPosition player_position;
};
