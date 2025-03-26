#pragma once
#include "math/transform.h"
#include "memory.h"
#include "renderer/tile.h"
#include <cassert>
#include <cmath>
#include <cstdint>

struct WorldPosition {
    uint32_t abs_tile_x;
    uint32_t abs_tile_y;
    float tile_rel_x;
    float tile_rel_y;

    Transform to_world_transform() {
        Transform transform;
        transform.set_position(
            // TODO: This only works if the tile side is 1.0f
            glm::vec3(abs_tile_x + tile_rel_x, 0.f, abs_tile_y + tile_rel_y));
        return transform;
    }
};

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

    // NOTE: We use 32x32 tile chunks
    uint32_t chunk_shift;
    uint32_t chunk_mask;
    uint32_t chunk_dim;

    uint32_t n_tile_chunk_x;
    uint32_t n_tile_chunk_y;

    TileChunk *tile_chunks;
};

struct World {
    TileMap *tile_map;
};

bool is_world_point_traversible(TileMap *tm, WorldPosition world_pos);
World *generate_world(Arena *arena);
std::vector<TileRenderingInput> create_tile_map_mesh(TileMap *tm);

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

    if (tile_chunk_x < tm->n_tile_chunk_x &&
        tile_chunk_y < tm->n_tile_chunk_y) {
        tile_chunk =
            &tm->tile_chunks[tm->n_tile_chunk_x * tile_chunk_y + tile_chunk_x];
    }

    return tile_chunk;
}
