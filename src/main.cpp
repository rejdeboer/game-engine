#include "SDL_events.h"
#include "SDL_mixer.h"
#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <cmath>
#include <cstdio>

#define internal static
#define f32 float
#define f64 double
#define i32 int
#define u32 unsigned int

#define SCREEN_WIDTH 640.
#define SCREEN_HEIGHT 480.
#define PLAYER_HEIGHT (SCREEN_HEIGHT / 8.)
#define PLAYER_SPEED ((double)SCREEN_HEIGHT / 200.)

Uint64 TIMESTEP_MS = 1000 / 60;

SDL_Window *gWindow = NULL;
SDL_Renderer *gRenderer = NULL;

struct TileMap {
  u32 n_rows;
  u32 n_columns;
  f32 start_x;
  f32 start_y;
  f32 tile_width;
  f32 tile_height;
  u32 *tiles;
};

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

  double player1Pos = 0.0;
  double player2Pos = 0.0;
  bool player1Down = false;
  bool player1Up = false;
  bool player2Down = false;
  bool player2Up = false;

  Uint64 next_game_step = SDL_GetTicks64();

  const f32 tile_width = 50.0f;
  const f32 tile_height = 50.0f;
  const u32 n_tile_rows = 9;
  const u32 n_tile_columns = 17;
  const f32 starting_tile_x = 0.0f;
  const f32 starting_tile_y = 0.0f;

  u32 tile_map[n_tile_rows][n_tile_columns] = {
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
  };

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
            player1Up = true;
            break;

          case SDLK_s:
            player1Down = true;
            break;

          case SDLK_UP:
            player2Up = true;
            break;

          case SDLK_DOWN:
            player2Down = true;
            break;

          default:
            break;
          }
        } else if (e.type == SDL_KEYUP) {
          switch (e.key.keysym.sym) {
          case SDLK_w:
            player1Up = false;
            break;

          case SDLK_s:
            player1Down = false;
            break;

          case SDLK_UP:
            player2Up = false;
            break;

          case SDLK_DOWN:
            player2Down = false;
            break;

          default:
            break;
          }
        }
      }

      if (player1Up && !player1Down) {
        player1Pos = player1Pos - PLAYER_SPEED;
      } else if (!player1Up && player1Down) {
        player1Pos = player1Pos + PLAYER_SPEED;
      }

      if (player2Up && !player2Down) {
        player2Pos = player2Pos - PLAYER_SPEED;
      } else if (!player2Up && player2Down) {
        player2Pos = player2Pos + PLAYER_SPEED;
      }

      next_game_step += TIMESTEP_MS;
    }

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(gRenderer);
    SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xFF);

    draw_rect(gRenderer, 0.0f,
              (SCREEN_HEIGHT / 2.0f + player1Pos) - (PLAYER_HEIGHT / 2.0f),
              SCREEN_WIDTH / 32.0f, PLAYER_HEIGHT, 0.0f, 0.0f, 0.0f);
    draw_rect(gRenderer, SCREEN_WIDTH - SCREEN_WIDTH / 32.0f,
              (SCREEN_HEIGHT / 2.0f + player2Pos) - (PLAYER_HEIGHT / 2.0f),
              SCREEN_WIDTH / 32.0f, PLAYER_HEIGHT, 0.0f, 0.0f, 0.0f);

    for (int row = 0; row < n_tile_rows; row++) {
      for (int col = 0; col < n_tile_columns; col++) {
        f32 tile_x = starting_tile_x + col * tile_width;
        f32 tile_y = starting_tile_y + row * tile_height;

        if (tile_map[row][col]) {
          draw_rect(gRenderer, tile_x, tile_y, tile_width, tile_height, 0.0f,
                    0.0f, 0.0f);
        } else {
          draw_rect(gRenderer, tile_x, tile_y, tile_width, tile_height, 1.0f,
                    1.0f, 1.0f);
        }
      }
    }

    SDL_RenderPresent(gRenderer);
  }
  SDL_DestroyWindow(gWindow);

  SDL_Quit();
  return 0;
}
