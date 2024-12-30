#include "SDL_events.h"
#include <SDL2/SDL.h>
#include <cstdio>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define PLAYER_HEIGHT (SCREEN_HEIGHT / 8)
#define PLAYER_SPEED (SCREEN_HEIGHT / 100)

SDL_Window *gWindow = NULL;
SDL_Renderer *gRenderer = NULL;

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

  int player1Pos = 0;
  int player2Pos = 0;
  bool player1Down = false;
  bool player1Up = false;
  bool player2Down = false;
  bool player2Up = false;

  // Hack to get window to stay up
  SDL_Event e;
  bool quit = false;
  while (quit == false) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        quit = true;
      } else if (e.type == SDL_KEYDOWN) {
        // Select surfaces based on key press
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
      }
    }

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(gRenderer);

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

    SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_Rect player1Rect = {
        0, (SCREEN_HEIGHT / 2 + player1Pos) - (PLAYER_HEIGHT / 2),
        SCREEN_WIDTH / 32, PLAYER_HEIGHT};
    SDL_RenderFillRect(gRenderer, &player1Rect);
    SDL_Rect player2Rect = {SCREEN_WIDTH - SCREEN_WIDTH / 32,
                            (SCREEN_HEIGHT / 2 + player2Pos) -
                                (PLAYER_HEIGHT / 2),
                            SCREEN_WIDTH / 32, PLAYER_HEIGHT};
    SDL_RenderFillRect(gRenderer, &player2Rect);

    SDL_RenderPresent(gRenderer);
  }
  SDL_DestroyWindow(gWindow);

  SDL_Quit();
  return 0;
}
