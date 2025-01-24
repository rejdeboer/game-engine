#if !defined(TILE_H)
#include <cstdint>

struct TileChunk {
  uint32_t *tiles;
};

struct TileChunkPosition {
  uint32_t chunk_x;
  uint32_t chunk_y;
  uint32_t tile_x;
  uint32_t tile_y;
};

struct TileMap {
  float tile_side_in_meters;
  int tile_side_in_pixels;
  float meters_to_pixels;

  // NOTE: We use 256x256 tile chunks
  uint32_t chunk_shift;
  uint32_t chunk_mask;
  uint32_t chunk_dim;

  uint32_t n_tile_chunk_x;
  uint32_t n_tile_chunk_y;

  TileChunk *tile_chunks;
};

#define TILE_H
#endif
