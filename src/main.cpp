#include "main.h"
#include "SDL_events.h"
#include "SDL_mixer.h"
#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_render.h>
#include <cassert>
#include <cmath>
#include <cstdio>

Uint64 TIMESTEP_MS = 1000 / 60;
f32 TIMESTEP_S = (f32)TIMESTEP_MS / 1000;

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

inline void normalize_world_coord(World *world, u32 tile_count, u32 *tile_map,
                                  i32 *tile, f32 *tile_rel) {
  i32 offset = (i32)floor(*tile_rel / world->tile_side_in_meters);
  *tile += offset;
  *tile_rel -= (f32)offset * world->tile_side_in_meters;

  assert(*tile_rel >= 0.0);
  assert(*tile_rel <= world->tile_side_in_meters);

  if (*tile < 0) {
    --*tile_map;
    *tile += tile_count;
  } else if (*tile >= tile_count) {
    ++*tile_map;
    *tile -= tile_count;
  }
}

inline WorldPosition normalize_world_position(World *world, WorldPosition pos) {
  WorldPosition res = pos;
  normalize_world_coord(world, world->n_columns, &res.tile_map_x, &res.tile_x,
                        &res.tile_rel_x);
  normalize_world_coord(world, world->n_rows, &res.tile_map_y, &res.tile_y,
                        &res.tile_rel_y);
  return res;
}

internal inline u32 get_tile_value(World *world, WorldPosition pos) {
  pos = normalize_world_position(world, pos);
  return world->tile_maps[pos.tile_map_y * world->n_tile_map_x + pos.tile_map_x]
      .tiles[pos.tile_y * world->n_columns + pos.tile_x];
}

internal inline bool is_tile_point_traversible(World *world,
                                               WorldPosition pos) {
  return get_tile_value(world, pos) == 0;
}

inline TileMap *get_tile_map(World *world, WorldPosition pos) {
  TileMap *tile_map = 0;

  if (pos.tile_map_x >= 0 && pos.tile_map_y >= 0 &&
      pos.tile_map_x < world->n_tile_map_x &&
      pos.tile_map_y < world->n_tile_map_y) {
    tile_map =
        &world
             ->tile_maps[world->n_tile_map_x * pos.tile_map_y + pos.tile_map_x];
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
  if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT,
                    MIX_DEFAULT_CHANNELS, 2048) < 0) {
    printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n",
           Mix_GetError());
    return 1;
  }

  WorldPosition player_position = {0, 0, 3, 3, 0.0f, 0.0f};
  GameState game_state = {player_position};
  bool playerDown = false;
  bool playerUp = false;
  bool playerLeft = false;
  bool playerRight = false;

  Uint64 next_game_step = SDL_GetTicks64();

  const u32 n_tile_rows = 9;
  const u32 n_tile_columns = 17;

  TileMap tile_maps[2][2];
  World world;
  world.tile_side_in_meters = 1.4f;
  world.tile_side_in_pixels = 60;
  world.meters_to_pixels =
      world.tile_side_in_pixels / (f32)world.tile_side_in_meters;
  world.tile_maps = (TileMap *)tile_maps;
  world.start_x = 0.0f;
  world.start_y = SCREEN_HEIGHT;
  world.n_rows = n_tile_rows;
  world.n_columns = n_tile_columns;
  world.n_tile_map_x = 2;
  world.n_tile_map_y = 2;

  u32 tiles_00[n_tile_rows][n_tile_columns] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
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
  world.tile_maps[0].tiles = (u32 *)tiles_00;
  world.tile_maps[1].tiles = (u32 *)&tiles_10;
  world.tile_maps[2].tiles = (u32 *)&tiles_10;
  world.tile_maps[3].tiles = (u32 *)&tiles_10;

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
        if (is_tile_point_traversible(&world, test_left) &&
            is_tile_point_traversible(&world, test_right)) {
          game_state.player_position.tile_rel_y = new_y;
          game_state.player_position =
              normalize_world_position(&world, game_state.player_position);
        }
      }

      if (playerLeft && !playerRight) {
        WorldPosition test_x = game_state.player_position;
        test_x.tile_rel_x -= dx + PLAYER_WIDTH / 2.0f;
        if (is_tile_point_traversible(&world, test_x)) {
          game_state.player_position.tile_rel_x -= dx;
          game_state.player_position =
              normalize_world_position(&world, game_state.player_position);
        }
      } else if (!playerLeft && playerRight) {
        WorldPosition test_x = game_state.player_position;
        test_x.tile_rel_x += dx + PLAYER_WIDTH / 2.0f;
        if (is_tile_point_traversible(&world, test_x)) {
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

    TileMap *tm = get_tile_map(&world, game_state.player_position);
    for (int row = 0; row < world.n_rows; row++) {
      for (int col = 0; col < world.n_columns; col++) {
        f32 tile_x = world.start_x + col * world.tile_side_in_pixels;
        f32 tile_y = world.start_y - row * world.tile_side_in_pixels;

        if (tm->tiles[row * world.n_columns + col]) {
          draw_rect(gRenderer, tile_x, tile_y, world.tile_side_in_pixels,
                    -world.tile_side_in_pixels, 0.0f, 0.0f, 0.0f);
        } else {
          draw_rect(gRenderer, tile_x, tile_y, world.tile_side_in_pixels,
                    -world.tile_side_in_pixels, 1.0f, 1.0f, 1.0f);
        }
      }
    }

    f32 player_left_x =
        world.tile_side_in_meters * game_state.player_position.tile_x +
        world.start_x + game_state.player_position.tile_rel_x -
        PLAYER_WIDTH / 2.0f;
    player_left_x *= world.meters_to_pixels;
    f32 player_bottom_y =
        world.start_y -
        (world.tile_side_in_pixels * game_state.player_position.tile_y +
         game_state.player_position.tile_rel_y * world.meters_to_pixels);
    draw_rect(gRenderer, player_left_x, player_bottom_y,
              PLAYER_WIDTH * world.meters_to_pixels,
              -PLAYER_HEIGHT * world.meters_to_pixels, 1.0f, 0.0f, 0.0f);

    SDL_RenderPresent(gRenderer);
  }
  SDL_DestroyWindow(gWindow);

  SDL_Quit();
  return 0;
}
