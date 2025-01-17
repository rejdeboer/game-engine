#include "main.h"
#include "SDL_events.h"
#include "SDL_mixer.h"
#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_render.h>
#include <cmath>
#include <cstdio>

Uint64 TIMESTEP_MS = 1000 / 60;

SDL_Window *gWindow = NULL;
SDL_Renderer *gRenderer = NULL;

internal void draw_rect(SDL_Renderer *renderer, f32 x_real, f32 y_real,
                        f32 width_real, f32 height_real, f32 r, f32 g, f32 b) {
  i32 x = (int)round(x_real);
  i32 y = (int)round(y_real);
  i32 width = (int)round(width_real);
  i32 height = (int)round(height_real);

  SDL_SetRenderDrawColor(renderer, (int)round(255.0f * r),
                         (int)round(255.0f * g), (int)round(255.0f * b), 0xFF);

  SDL_Rect rect = {x, y, width, height};
  SDL_RenderFillRect(renderer, &rect);
}

internal bool is_tile_point_traversible(TileMap *tm, f32 x, f32 y) {
  f32 tile_x = x - tm->start_x;
  f32 tile_y = y - tm->start_y;

  u32 row = (u32)(tile_y / tm->tile_height);
  u32 col = (u32)(tile_x / tm->tile_width);

  return tm->tiles[row * tm->n_columns + col] == 0;
}

inline TileMap *get_tile_map(World *world, u32 tile_map_x, u32 tile_map_y) {
  TileMap *tile_map = 0;

  if (tile_map_x >= 0 && tile_map_y >= 0 && tile_map_x < world->n_tile_map_x &&
      tile_map_y < world->n_tile_map_y) {
    tile_map = &world->tile_maps[world->n_tile_map_x * tile_map_y + tile_map_x];
  }

  return tile_map;
}

int main(int argc, char *args[]) {
  SDL_Surface *screenSurface = NULL;
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "could not initialize sdl2: %s\n", SDL_GetError());
    return 1;
  }
  gWindow = SDL_CreateWindow("hello_sdl2", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
                             SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
  if (gWindow == NULL) {
    fprintf(stderr, "could not create window: %s\n", SDL_GetError());
    return 1;
  }

  gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
  if (gRenderer == NULL) {
    fprintf(stderr, "could not create renderer: %s\n", SDL_GetError());
    return 1;
  }

  // Initialize SDL_mixer
  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n",
           Mix_GetError());
    return 1;
  }

  GameState game_state = {0, 0, 80.0f, 80.0f};
  bool playerDown = false;
  bool playerUp = false;
  bool playerLeft = false;
  bool playerRight = false;

  Uint64 next_game_step = SDL_GetTicks64();

  const u32 n_tile_rows = 9;
  const u32 n_tile_columns = 17;

  TileMap tile_maps[2][2];
  World world = {2, 2, (TileMap *)tile_maps};

  u32 tiles_00[n_tile_rows][n_tile_columns] = {
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
  u32 tiles_10[n_tile_rows][n_tile_columns] = {
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
  tile_maps[0][0].n_rows = n_tile_rows;
  tile_maps[0][0].n_columns = n_tile_columns;
  tile_maps[0][0].start_x = 0.0f;
  tile_maps[0][0].start_y = 0.0f;
  tile_maps[0][0].tile_width = 50.0f;
  tile_maps[0][0].tile_height = 50.0f;
  tile_maps[0][0].tiles = (u32 *)tiles_00;

  tile_maps[0][1] = tile_maps[0][0];
  tile_maps[0][1].tiles = (u32 *)&tiles_10;

  tile_maps[1][0] = tile_maps[0][0];
  tile_maps[1][0].tiles = (u32 *)&tiles_10;

  tile_maps[1][1] = tile_maps[0][0];
  tile_maps[1][1].tiles = (u32 *)&tiles_10;

  TileMap *current_tile_map = get_tile_map(&world, 0, 0);

  SDL_Event e;
  bool quit = false;
  while (quit == false) {
    Uint64 now = SDL_GetTicks64();
    if (next_game_step >= now) {
      SDL_Delay(next_game_step - now);
      continue;
    }

    while (next_game_step <= now) {
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
          quit = true;
        } else if (e.type == SDL_KEYDOWN) {
          switch (e.key.keysym.sym) {
          case SDLK_w:
          case SDLK_UP:
            playerUp = true;
            break;

          case SDLK_s:
          case SDLK_DOWN:
            playerDown = true;
            break;

          case SDLK_a:
          case SDLK_LEFT:
            playerLeft = true;
            break;

          case SDLK_d:
          case SDLK_RIGHT:
            playerRight = true;
            break;

          default:
            break;
          }
        } else if (e.type == SDL_KEYUP) {
          switch (e.key.keysym.sym) {
          case SDLK_w:
          case SDLK_UP:
            playerUp = false;
            break;

          case SDLK_s:
          case SDLK_DOWN:
            playerDown = false;
            break;

          case SDLK_a:
          case SDLK_LEFT:
            playerLeft = false;
            break;

          case SDLK_d:
          case SDLK_RIGHT:
            playerRight = false;
            break;

          default:
            break;
          }
        }
      }

      f32 new_y;
      if (playerUp && !playerDown) {
        new_y = game_state.player_y - PLAYER_SPEED;
      } else if (!playerUp && playerDown) {
        new_y = game_state.player_y + PLAYER_SPEED;
      }

      if (new_y &&
          is_tile_point_traversible(current_tile_map,
                                    game_state.player_x - PLAYER_WIDTH / 2.0f,
                                    new_y) &&
          is_tile_point_traversible(current_tile_map,
                                    game_state.player_x + PLAYER_WIDTH / 2.0f,
                                    new_y)) {
        game_state.player_y = new_y;
      }

      if (playerLeft && !playerRight) {
        f32 new_x = game_state.player_x - PLAYER_SPEED;
        if (is_tile_point_traversible(current_tile_map,
                                      new_x - PLAYER_WIDTH / 2.0f,
                                      game_state.player_y)) {
          game_state.player_x = new_x;
        }
      } else if (!playerLeft && playerRight) {
        f32 new_x = game_state.player_x + PLAYER_SPEED;
        if (is_tile_point_traversible(current_tile_map,
                                      new_x + PLAYER_WIDTH / 2.0f,
                                      game_state.player_y)) {
          game_state.player_x = new_x;
        }
      }

      next_game_step += TIMESTEP_MS;
    }

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(gRenderer);
    SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xFF);

    f32 player_left_x = game_state.player_x - PLAYER_WIDTH / 2.0f;
    f32 player_bottom_y = game_state.player_y;

    for (int row = 0; row < current_tile_map->n_rows; row++) {
      for (int col = 0; col < current_tile_map->n_columns; col++) {
        f32 tile_x =
            current_tile_map->start_x + col * current_tile_map->tile_width;
        f32 tile_y =
            current_tile_map->start_y + row * current_tile_map->tile_height;

        if (current_tile_map->tiles[row * current_tile_map->n_columns + col]) {
          draw_rect(gRenderer, tile_x, tile_y, current_tile_map->tile_width,
                    current_tile_map->tile_height, 0.0f, 0.0f, 0.0f);
        } else {
          draw_rect(gRenderer, tile_x, tile_y, current_tile_map->tile_width,
                    current_tile_map->tile_height, 1.0f, 1.0f, 1.0f);
        }
      }
    }

    draw_rect(gRenderer, player_left_x, player_bottom_y, PLAYER_WIDTH,
              -PLAYER_HEIGHT, 1.0f, 0.0f, 0.0f);

    SDL_RenderPresent(gRenderer);
  }
  SDL_DestroyWindow(gWindow);

  SDL_Quit();
  return 0;
}
