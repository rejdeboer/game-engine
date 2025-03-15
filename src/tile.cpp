#include "tile.h"
#include <vector>

static inline uint32_t get_tile_value(TileMap *tm, TileChunk *tc,
                                      uint32_t tile_x, uint32_t tile_y) {
    assert(tc);
    assert(tile_x < tm->chunk_dim);
    assert(tile_y < tm->chunk_dim);
    return tc->tiles[tile_y * tm->chunk_dim + tile_x];
}

static inline bool is_chunk_tile_traversible(TileMap *tm, TileChunk *tc,
                                             uint32_t tile_x, uint32_t tile_y) {
    if (tc) {
        return get_tile_value(tm, tc, tile_x, tile_y) == 0;
    }
    return false;
}

bool is_world_point_traversible(TileMap *tm, WorldPosition world_pos) {
    world_pos = normalize_world_position(tm, world_pos);
    TileChunkPosition chunk_pos =
        get_chunk_position(tm, world_pos.abs_tile_x, world_pos.abs_tile_y);
    TileChunk *chunk = get_tile_chunk(tm, chunk_pos.chunk_x, chunk_pos.chunk_y);
    return is_chunk_tile_traversible(tm, chunk, chunk_pos.tile_x,
                                     chunk_pos.tile_y);
}

World *generate_world(Arena *arena) {
    const uint32_t n_tile_rows = 9;
    const uint32_t n_tile_columns = 17;

    TileChunk tile_chunks[2][2];
    World *world = push_size<World>(arena);
    world->tile_map = push_size<TileMap>(arena);
    TileMap *tile_map = world->tile_map;
    tile_map->tile_side_in_meters = 1.4f;
    tile_map->tile_side_in_pixels = 60;
    tile_map->meters_to_pixels =
        tile_map->tile_side_in_pixels / (float)tile_map->tile_side_in_meters;
    tile_map->chunk_shift = 8;
    tile_map->chunk_mask = (1 << tile_map->chunk_shift) - 1;
    tile_map->chunk_dim = (1 << tile_map->chunk_shift);
    tile_map->tile_chunks = (TileChunk *)tile_chunks;
    tile_map->n_tile_chunk_x = 2;
    tile_map->n_tile_chunk_y = 2;

    tile_map->tile_chunks = push_array<TileChunk>(
        arena, tile_map->n_tile_chunk_x * tile_map->n_tile_chunk_y);
    for (int row = 0; row < tile_map->n_tile_chunk_y; row++) {
        for (int col = 0; col < tile_map->n_tile_chunk_x; col++) {
            // TODO: This is temporary junk
            tile_map->tile_chunks[row * tile_map->n_tile_chunk_x + col].tiles =
                push_array<uint32_t>(arena,
                                     tile_map->chunk_dim * tile_map->chunk_dim);
            for (int i = 0; i < tile_map->chunk_dim; i++) {
                for (int j = 0; j < tile_map->chunk_dim; j++) {
                    if (i == 0 || j == 0) {
                        tile_map
                            ->tile_chunks[row * tile_map->n_tile_chunk_x + col]
                            .tiles[i * tile_map->chunk_dim + j] = 1;
                    } else {
                        tile_map
                            ->tile_chunks[row * tile_map->n_tile_chunk_x + col]
                            .tiles[i * tile_map->chunk_dim + j] = 0;
                    }
                }
            }
        }
    }

    return world;
}

static std::vector<TileInstance> create_tile_chunk_mesh(TileChunk *chunk,
                                                        uint32_t chunkDim,
                                                        uint32_t tileWidth) {
    std::vector<TileInstance> instances(chunkDim * chunkDim);

    for (uint32_t row = 0; row < chunkDim; row++) {
        for (uint32_t col = 0; col < chunkDim; col++) {
            TileInstance instance;
            instance.pos.x = (float)col;
            instance.pos.y = (float)row;
            instance.color.x = (float)(col % 2);
            instance.color.y = (float)(row % 2);
            instance.color.z = 1;
            instances[row * chunkDim + col] = instance;
        }
    }

    return instances;
}

std::vector<TileRenderingInput> create_tile_map_mesh(TileMap *tm) {
    std::vector<TileRenderingInput> chunks(tm->n_tile_chunk_x *
                                           tm->n_tile_chunk_y);

    for (uint32_t row = 0; row < tm->n_tile_chunk_y; row++) {
        for (uint32_t col = 0; col < tm->n_tile_chunk_x; col++) {
            TileRenderingInput chunk;
            chunk.instances = create_tile_chunk_mesh(
                &tm->tile_chunks[row * tm->n_tile_chunk_x + col], tm->chunk_dim,
                tm->tile_side_in_pixels);
            chunk.chunkPosition = glm::vec3(col, row, 0.f);
            chunks[row * tm->n_tile_chunk_x + col] = chunk;
        }
    }

    return chunks;
}
