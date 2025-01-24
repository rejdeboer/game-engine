#include "tile.h"
#include <cassert>
#include <cmath>

inline void normalize_world_coord(TileMap *tm, uint32_t *tile,
                                  float *tile_rel) {
  int offset = (int)roundf(*tile_rel / tm->tile_side_in_meters);
  *tile += offset;
  *tile_rel -= (float)offset * tm->tile_side_in_meters;

  assert(*tile_rel >= -tm->tile_side_in_meters / 2.0f);
  assert(*tile_rel <= tm->tile_side_in_meters / 2.0f);
}

inline WorldPosition normalize_world_position(TileMap *tm, WorldPosition pos) {
  WorldPosition res = pos;
  normalize_world_coord(tm, &res.abs_tile_x, &res.tile_rel_x);
  normalize_world_coord(tm, &res.abs_tile_y, &res.tile_rel_y);
  return res;
}

static inline uint32_t get_tile_value(TileMap *tm, TileChunk *tc,
                                      uint32_t tile_x, uint32_t tile_y) {
  assert(tc);
  assert(tile_x < tm->chunk_dim);
  assert(tile_y < tm->chunk_dim);
  return tc->tiles[tile_y * tm->chunk_dim + tile_x];
}

inline TileChunkPosition get_chunk_position(TileMap *tm, uint32_t abs_tile_x,
                                            uint32_t abs_tile_y) {
  TileChunkPosition res;
  res.tile_x = abs_tile_x & tm->chunk_mask;
  res.tile_y = abs_tile_y & tm->chunk_mask;
  res.chunk_x = abs_tile_x >> tm->chunk_shift;
  res.chunk_y = abs_tile_y >> tm->chunk_shift;
  return res;
}

inline TileChunk *get_tile_chunk(TileMap *tm, uint32_t tile_chunk_x,
                                 uint32_t tile_chunk_y) {
  TileChunk *tile_chunk = 0;

  if (tile_chunk_x < tm->n_tile_chunk_x && tile_chunk_y < tm->n_tile_chunk_y) {
    tile_chunk =
        &tm->tile_chunks[tm->n_tile_chunk_x * tile_chunk_y + tile_chunk_x];
  }

  return tile_chunk;
}

static inline bool is_chunk_tile_traversible(TileMap *tm, TileChunk *tc,
                                             uint32_t tile_x, uint32_t tile_y) {
  if (tc) {
    return get_tile_value(tm, tc, tile_x, tile_y) == 0;
  }
  return false;
}

static bool is_world_point_traversible(TileMap *tm, WorldPosition world_pos) {
  world_pos = normalize_world_position(tm, world_pos);
  TileChunkPosition chunk_pos =
      get_chunk_position(tm, world_pos.abs_tile_x, world_pos.abs_tile_y);
  TileChunk *chunk = get_tile_chunk(tm, chunk_pos.chunk_x, chunk_pos.chunk_y);
  return is_chunk_tile_traversible(tm, chunk, chunk_pos.tile_x,
                                   chunk_pos.tile_y);
}
