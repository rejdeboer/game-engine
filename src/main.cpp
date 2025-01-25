#include "main.h"
#include "memory.cpp"
#include "tile.cpp"
#include <SDL3/SDL.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

Uint64 TIMESTEP_MS = 1000 / 60;
float TIMESTEP_S = (float)TIMESTEP_MS / 1000;

SDL_Window *gWindow = NULL;
SDL_Renderer *gRenderer = NULL;

static void draw_rect(SDL_Renderer *renderer, float x_real, float y_real,
                      float width_real, float height_real, float r, float g,
                      float b) {
  float x = round(x_real);
  float y = round(y_real);
  float width = round(width_real);
  float height = round(height_real);

  SDL_SetRenderDrawColor(renderer, (int)round(255.0f * r),
                         (int)round(255.0f * g), (int)round(255.0f * b), 0xFF);

  SDL_FRect rect = {x, y, width, height};
  SDL_RenderFillRect(renderer, &rect);
}

int main(int argc, char *args[]) {
  SDL_Surface *screenSurface = NULL;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "could not initialize sdl3: %s\n", SDL_GetError());
    return 1;
  }
  gWindow = SDL_CreateWindow("hello_sdl3", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
  if (gWindow == NULL) {
    fprintf(stderr, "could not create window: %s\n", SDL_GetError());
    return 1;
  }

  gRenderer = SDL_CreateRenderer(gWindow, NULL);
  if (gRenderer == NULL) {
    fprintf(stderr, "could not create renderer: %s\n", SDL_GetError());
    return 1;
  }

  void *memory = malloc(GAME_MEMORY);
  Arena arena;
  arena_init(&arena, GAME_MEMORY, (uint8_t *)memory);

  // TODO: Should everything be allocated inside the game memory
  WorldPosition player_position = {3, 3, 0.0f, 0.0f};
  GameState game_state = {arena, player_position};
  bool playerDown = false;
  bool playerUp = false;
  bool playerLeft = false;
  bool playerRight = false;

  Uint64 next_game_step = SDL_GetTicks();

  const uint32_t n_tile_rows = 9;
  const uint32_t n_tile_columns = 17;
  World *world = generate_world(&arena);
  TileMap *tile_map = world->tile_map;

  float lower_left_x = 0.0f;
  float lower_left_y = SCREEN_HEIGHT;

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

      float dx = PLAYER_SPEED * TIMESTEP_S;
      float dy = PLAYER_SPEED * TIMESTEP_S;

      float new_y;
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
        if (is_world_point_traversible(world->tile_map, test_left) &&
            is_world_point_traversible(world->tile_map, test_right)) {
          game_state.player_position.tile_rel_y = new_y;
          game_state.player_position = normalize_world_position(
              world->tile_map, game_state.player_position);
        }
      }

      if (playerLeft && !playerRight) {
        WorldPosition test_x = game_state.player_position;
        test_x.tile_rel_x -= dx + PLAYER_WIDTH / 2.0f;
        if (is_world_point_traversible(world->tile_map, test_x)) {
          game_state.player_position.tile_rel_x -= dx;
          game_state.player_position = normalize_world_position(
              world->tile_map, game_state.player_position);
        }
      } else if (!playerLeft && playerRight) {
        WorldPosition test_x = game_state.player_position;
        test_x.tile_rel_x += dx + PLAYER_WIDTH / 2.0f;
        if (is_world_point_traversible(world->tile_map, test_x)) {
          game_state.player_position.tile_rel_x += dx;
          game_state.player_position = normalize_world_position(
              world->tile_map, game_state.player_position);
        }
      }

      next_game_step += TIMESTEP_MS;
    }

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(gRenderer);
    SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xFF);

    // TODO: n_tiles should not depend on window size
    uint32_t n_tiles_x =
        (uint32_t)ceilf(SCREEN_WIDTH / tile_map->tile_side_in_pixels) + 1;
    uint32_t n_tiles_y =
        (uint32_t)ceilf(SCREEN_HEIGHT / tile_map->tile_side_in_pixels) + 1;

    uint32_t player_abs_tile_x = game_state.player_position.abs_tile_x;
    uint32_t player_abs_tile_y = game_state.player_position.abs_tile_y;
    uint32_t center_tile_x = n_tiles_x / 2;
    uint32_t center_tile_y = n_tiles_y / 2;
    float tile_offset_pixels_x =
        game_state.player_position.tile_rel_x * tile_map->meters_to_pixels +
        0.5f * tile_map->tile_side_in_pixels;
    float tile_offset_pixels_y =
        game_state.player_position.tile_rel_y * tile_map->meters_to_pixels +
        0.5f * tile_map->tile_side_in_pixels;
    for (int row = 0; row < n_tiles_y; row++) {
      for (int col = 0; col < n_tiles_x; col++) {
        float tile_x = lower_left_x - tile_offset_pixels_x +
                       col * tile_map->tile_side_in_pixels;
        float tile_y = lower_left_y + tile_offset_pixels_y -
                       row * tile_map->tile_side_in_pixels;

        uint32_t abs_tile_x = player_abs_tile_x + col - center_tile_x;
        uint32_t abs_tile_y = player_abs_tile_y + row - center_tile_y;
        TileChunkPosition chunk_pos =
            get_chunk_position(world->tile_map, abs_tile_x, abs_tile_y);
        TileChunk *chunk = get_tile_chunk(world->tile_map, chunk_pos.chunk_x,
                                          chunk_pos.chunk_y);

        if (chunk && chunk->tiles[chunk_pos.tile_y * tile_map->chunk_dim +
                                  chunk_pos.tile_x]) {
          draw_rect(gRenderer, tile_x, tile_y, tile_map->tile_side_in_pixels,
                    -tile_map->tile_side_in_pixels, 0.0f, 0.0f, 0.0f);
        } else {
          draw_rect(gRenderer, tile_x, tile_y, tile_map->tile_side_in_pixels,
                    -tile_map->tile_side_in_pixels, 1.0f, 1.0f, 1.0f);
        }
      }
    }

    // TODO: Calculate player position based on state
    float player_left_x = SCREEN_WIDTH / 2.0f -
                          (PLAYER_WIDTH / 2.0f) * tile_map->meters_to_pixels;
    float player_bottom_y =
        SCREEN_HEIGHT / 2.0f - tile_map->tile_side_in_pixels / 2.0f;
    draw_rect(gRenderer, player_left_x, player_bottom_y,
              PLAYER_WIDTH * tile_map->meters_to_pixels,
              -PLAYER_HEIGHT * tile_map->meters_to_pixels, 1.0f, 0.0f, 0.0f);

    SDL_RenderPresent(gRenderer);
  }
  SDL_DestroyWindow(gWindow);

  SDL_Quit();
  free(memory);
  return 0;
}
