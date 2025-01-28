#include "SDL3/SDL_rect.h"
#include "SDL3/SDL_surface.h"
#include "SDL3_image/SDL_image.h"
#include "main.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <cassert>
#include <cmath>
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
                           (int)round(255.0f * g), (int)round(255.0f * b),
                           0xFF);

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

    SDL_Surface *test_surface = IMG_Load("./assets/images/test-isometric.png");
    if (test_surface == NULL) {
        fprintf(stderr,
                "Texture error: Could not load image! SDL_image Error:%s\n",
                SDL_GetError());
        return 1;
    }
    SDL_Texture *texture =
        SDL_CreateTextureFromSurface(gRenderer, test_surface);
    if (texture == NULL) {
        fprintf(stderr,
                "Texture error: Could not load image! SDL_image Error:%s\n",
                SDL_GetError());
        return 1;
    }
    SDL_DestroySurface(test_surface);
    SDL_FRect tex_rect = {0.0f, 0.0f, 64.0f, 64.0f};
    SDL_FRect dest_rect = {50.0f, 50.0f, 50.0f, 50.0f};
    SDL_FPoint center = {25.0f, 25.0f};

    Uint64 next_game_step = SDL_GetTicks();
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
                }
            }

            next_game_step += TIMESTEP_MS;
        }

        SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderClear(gRenderer);
        SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xFF);

        draw_rect(gRenderer, 50.0f, 50.0f, 50.0f, 50.0f, 1.0f, 0.0f, 0.0f);
        SDL_RenderTextureRotated(gRenderer, texture, &tex_rect, &dest_rect,
                                 45.0f, &center, SDL_FlipMode::SDL_FLIP_NONE);

        SDL_RenderPresent(gRenderer);
    }
    SDL_DestroyWindow(gWindow);

    SDL_Quit();
    return 0;
}
