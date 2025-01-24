#include "main.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <cassert>
#include <cmath>
#include <cstdio>

Uint64 TIMESTEP_MS = 1000 / 60;
f32 TIMESTEP_S = (f32)TIMESTEP_MS / 1000;

SDL_Window *gWindow = NULL;
SDL_Renderer *gRenderer = NULL;

static void draw_rect(SDL_Renderer *renderer, f32 x_real, f32 y_real,
                      f32 width_real, f32 height_real, f32 r, f32 g, f32 b) {
  f32 x = round(x_real);
  f32 y = round(y_real);
  f32 width = round(width_real);
  f32 height = round(height_real);

  SDL_SetRenderDrawColor(renderer, (int)round(255.0f * r),
                         (int)round(255.0f * g), (int)round(255.0f * b), 0xFF);

  SDL_FRect rect = {x, y, width, height};
  SDL_RenderFillRect(renderer, &rect);
}

inline void normalize_world_coord(World *world, u32 *tile, f32 *tile_rel) {
  i32 offset = (i32)roundf(*tile_rel / world->tile_side_in_meters);
  *tile += offset;
  *tile_rel -= (f32)offset * world->tile_side_in_meters;

  assert(*tile_rel >= -world->tile_side_in_meters / 2.0f);
  assert(*tile_rel <= world->tile_side_in_meters / 2.0f);
}

inline WorldPosition normalize_world_position(World *world, WorldPosition pos) {
  WorldPosition res = pos;
  normalize_world_coord(world, &res.abs_tile_x, &res.tile_rel_x);
  normalize_world_coord(world, &res.abs_tile_y, &res.tile_rel_y);
  return res;
}

static inline u32 get_tile_value(World *world, TileChunk *tc, u32 tile_x,
                                 u32 tile_y) {
  assert(tc);
  assert(tile_x < world->chunk_dim);
  assert(tile_y < world->chunk_dim);
  return tc->tiles[tile_y * world->chunk_dim + tile_x];
}

inline TileChunkPosition get_chunk_position(World *world, u32 abs_tile_x,
                                            u32 abs_tile_y) {
  TileChunkPosition res;
  res.tile_x = abs_tile_x & world->chunk_mask;
  res.tile_y = abs_tile_y & world->chunk_mask;
  res.chunk_x = abs_tile_x >> world->chunk_shift;
  res.chunk_y = abs_tile_y >> world->chunk_shift;
  return res;
}

inline TileChunk *get_tile_chunk(World *world, u32 tile_chunk_x,
                                 u32 tile_chunk_y) {
  TileChunk *tile_chunk = 0;

  if (tile_chunk_x < world->n_tile_chunk_x &&
      tile_chunk_y < world->n_tile_chunk_y) {
    tile_chunk =
        &world
             ->tile_chunks[world->n_tile_chunk_x * tile_chunk_y + tile_chunk_x];
  }

  return tile_chunk;
}

static inline bool is_chunk_tile_traversible(World *world, TileChunk *tc,
                                             u32 tile_x, u32 tile_y) {
  if (tc) {
    return get_tile_value(world, tc, tile_x, tile_y) == 0;
  }
  return false;
}

static bool is_world_point_traversible(World *world, WorldPosition world_pos) {
  world_pos = normalize_world_position(world, world_pos);
  TileChunkPosition chunk_pos =
      get_chunk_position(world, world_pos.abs_tile_x, world_pos.abs_tile_y);
  TileChunk *chunk =
      get_tile_chunk(world, chunk_pos.chunk_x, chunk_pos.chunk_y);
  return is_chunk_tile_traversible(world, chunk, chunk_pos.tile_x,
                                   chunk_pos.tile_y);
}

int main(int argc, char *args[]) {
  SDL_Surface *screenSurface = NULL;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "could not initialize sdl2: %s\n", SDL_GetError());
    return 1;
  }
  gWindow = SDL_CreateWindow("hello_sdl2", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
  if (gWindow == NULL) {
    fprintf(stderr, "could not create window: %s\n", SDL_GetError());
    return 1;
  }

  gRenderer = SDL_CreateRenderer(gWindow, NULL);
  if (gRenderer == NULL) {
    fprintf(stderr, "could not create renderer: %s\n", SDL_GetError());
    return 1;
  }

  WorldPosition player_position = {3, 3, 0.0f, 0.0f};
  GameState game_state = {player_position};
  bool playerDown = false;
  bool playerUp = false;
  bool playerLeft = false;
  bool playerRight = false;

  Uint64 next_game_step = SDL_GetTicks();

  const u32 n_tile_rows = 9;
  const u32 n_tile_columns = 17;

  TileChunk tile_chunks[2][2];
  World world;
  world.tile_side_in_meters = 1.4f;
  world.tile_side_in_pixels = 60;
  world.meters_to_pixels =
      world.tile_side_in_pixels / (f32)world.tile_side_in_meters;
  world.chunk_shift = 8;
  world.chunk_mask = (1 << world.chunk_shift) - 1;
  world.chunk_dim = 256;
  world.tile_chunks = (TileChunk *)tile_chunks;
  world.n_tile_chunk_x = 2;
  world.n_tile_chunk_y = 2;

  u32 tiles_00[256][256] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
  };
  world.tile_chunks[0].tiles = (u32 *)tiles_00;

  f32 lower_left_x = 0.0f;
  f32 lower_left_y = SCREEN_HEIGHT;

  SDL_Event e;
  bool quit = false;
  while (quit == false) {
    Uint64 now = SDL_GetTicks();
    if (next_game_step >= now) {
      SDL_Delay(next_game_step - now);
      continue;
    }

    while (next_game_step <= now) {
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
          quit = true;
        } else if (e.type == SDL_EVENT_KEY_DOWN) {
          switch (e.key.key) {
          case SDLK_W:
          case SDLK_UP:
            playerUp = true;
            break;

          case SDLK_S:
          case SDLK_DOWN:
            playerDown = true;
            break;

          case SDLK_A:
          case SDLK_LEFT:
            playerLeft = true;
            break;

          case SDLK_D:
          case SDLK_RIGHT:
            playerRight = true;
            break;

          default:
            break;
          }
        } else if (e.type == SDL_EVENT_KEY_UP) {
          switch (e.key.key) {
          case SDLK_W:
          case SDLK_UP:
            playerUp = false;
            break;

          case SDLK_S:
          case SDLK_DOWN:
            playerDown = false;
            break;

          case SDLK_A:
          case SDLK_LEFT:
            playerLeft = false;
            break;

          case SDLK_D:
          case SDLK_RIGHT:
            playerRight = false;
            break;

          default:
            break;
          }
        }
      }

      f32 dx = PLAYER_SPEED * TIMESTEP_S;
      f32 dy = PLAYER_SPEED * TIMESTEP_S;

      f32 new_y;
      if (playerUp && !playerDown) {
        new_y = game_state.player_position.tile_rel_y + dy;
      } else if (!playerUp && playerDown) {
        new_y = game_state.player_position.tile_rel_y - dy;
      }

      if (new_y) {
        WorldPosition test_left = game_state.player_position;
        WorldPosition test_right = game_state.player_position;
        test_left.tile_rel_y = new_y;
        test_right.tile_rel_y = new_y;
        test_left.tile_rel_x -= PLAYER_WIDTH / 2.0f;
        test_right.tile_rel_x += PLAYER_WIDTH / 2.0f;
        if (is_world_point_traversible(&world, test_left) &&
            is_world_point_traversible(&world, test_right)) {
          game_state.player_position.tile_rel_y = new_y;
          game_state.player_position =
              normalize_world_position(&world, game_state.player_position);
        }
      }

      if (playerLeft && !playerRight) {
        WorldPosition test_x = game_state.player_position;
        test_x.tile_rel_x -= dx + PLAYER_WIDTH / 2.0f;
        if (is_world_point_traversible(&world, test_x)) {
          game_state.player_position.tile_rel_x -= dx;
          game_state.player_position =
              normalize_world_position(&world, game_state.player_position);
        }
      } else if (!playerLeft && playerRight) {
        WorldPosition test_x = game_state.player_position;
        test_x.tile_rel_x += dx + PLAYER_WIDTH / 2.0f;
        if (is_world_point_traversible(&world, test_x)) {
          game_state.player_position.tile_rel_x += dx;
          game_state.player_position =
              normalize_world_position(&world, game_state.player_position);
        }
      }

      next_game_step += TIMESTEP_MS;
    }

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(gRenderer);
    SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xFF);

    // TODO: n_tiles should not depend on window size
    u32 n_tiles_x = (u32)ceilf(SCREEN_WIDTH / world.tile_side_in_pixels) + 1;
    u32 n_tiles_y = (u32)ceilf(SCREEN_HEIGHT / world.tile_side_in_pixels) + 1;

    u32 player_abs_tile_x = game_state.player_position.abs_tile_x;
    u32 player_abs_tile_y = game_state.player_position.abs_tile_y;
    u32 center_tile_x = n_tiles_x / 2;
    u32 center_tile_y = n_tiles_y / 2;
    f32 tile_offset_pixels_x =
        game_state.player_position.tile_rel_x * world.meters_to_pixels +
        0.5f * world.tile_side_in_pixels;
    f32 tile_offset_pixels_y =
        game_state.player_position.tile_rel_y * world.meters_to_pixels +
        0.5f * world.tile_side_in_pixels;
    for (int row = 0; row < n_tiles_y; row++) {
      for (int col = 0; col < n_tiles_x; col++) {
        f32 tile_x = lower_left_x - tile_offset_pixels_x +
                     col * world.tile_side_in_pixels;
        f32 tile_y = lower_left_y + tile_offset_pixels_y -
                     row * world.tile_side_in_pixels;

        u32 abs_tile_x = player_abs_tile_x + col - center_tile_x;
        u32 abs_tile_y = player_abs_tile_y + row - center_tile_y;
        TileChunkPosition chunk_pos =
            get_chunk_position(&world, abs_tile_x, abs_tile_y);
        TileChunk *chunk =
            get_tile_chunk(&world, chunk_pos.chunk_x, chunk_pos.chunk_y);

        if (chunk && chunk->tiles[chunk_pos.tile_y * world.chunk_dim +
                                  chunk_pos.tile_x]) {
          draw_rect(gRenderer, tile_x, tile_y, world.tile_side_in_pixels,
                    -world.tile_side_in_pixels, 0.0f, 0.0f, 0.0f);
        } else {
          draw_rect(gRenderer, tile_x, tile_y, world.tile_side_in_pixels,
                    -world.tile_side_in_pixels, 1.0f, 1.0f, 1.0f);
        }
      }
    }

    // TODO: Calculate player position based on state
    f32 player_left_x =
        SCREEN_WIDTH / 2.0f - (PLAYER_WIDTH / 2.0f) * world.meters_to_pixels;
    f32 player_bottom_y =
        SCREEN_HEIGHT / 2.0f - world.tile_side_in_pixels / 2.0f;
    draw_rect(gRenderer, player_left_x, player_bottom_y,
              PLAYER_WIDTH * world.meters_to_pixels,
              -PLAYER_HEIGHT * world.meters_to_pixels, 1.0f, 0.0f, 0.0f);

    SDL_RenderPresent(gRenderer);
  }
  SDL_DestroyWindow(gWindow);

  SDL_Quit();
  return 0;
}
