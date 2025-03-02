#include "imgui_impl_sdl3.h"
#include "main.h"
#include "renderer/renderer.hpp"
#include <SDL3/SDL_render.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vulkan/vulkan.h>

Uint64 TIMESTEP_MS = 1000 / 60;
float TIMESTEP_S = (float)TIMESTEP_MS / 1000;

SDL_Window *gWindow = NULL;

int main(int argc, char *args[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "could not initialize sdl3: %s\n", SDL_GetError());
        return 1;
    }
    gWindow = SDL_CreateWindow("hello_sdl3", SCREEN_WIDTH, SCREEN_HEIGHT,
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (gWindow == NULL) {
        fprintf(stderr, "could not create window: %s\n", SDL_GetError());
        return 1;
    }

    GameState state;
    Renderer vk_renderer = Renderer(gWindow);
    Camera camera;
    camera.position = glm::vec3(30.f, -00.f, -085.f);

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
                ImGui_ImplSDL3_ProcessEvent(&e);
                // TODO: Clean this up
                // camera.processSDLEvent(e);
                // camera.update();
                vk_renderer.set_camera_view(camera.get_view_matrix());
            }

            next_game_step += TIMESTEP_MS;
        }

        vk_renderer.draw_frame(&state);
        state.stats.frameTime = SDL_GetTicks() - now;
        state.stats.fps = 1000.f / (float)state.stats.frameTime;
    }
    vk_renderer.deinit();
    SDL_DestroyWindow(gWindow);

    SDL_Quit();
    return 0;
}
