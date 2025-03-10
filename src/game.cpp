#include "game.h"
#include <imgui_impl_sdl3.h>

Uint64 TIMESTEP_MS = 1000 / 60;
float TIMESTEP_S = (float)TIMESTEP_MS / 1000;

void Game::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "could not initialize sdl3: %s\n", SDL_GetError());
        abort();
    }

    _window = SDL_CreateWindow("hello_sdl3", SCREEN_WIDTH, SCREEN_HEIGHT,
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (_window == NULL) {
        fprintf(stderr, "could not create window: %s\n", SDL_GetError());
        abort();
    }

    _state.camera.position = glm::vec3(30.f, -00.f, -085.f);
}

void Game::deinit() {
    _renderer.deinit();
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

void Game::run() {
    Uint64 next_game_step = SDL_GetTicks();
    Uint64 last = next_game_step;
    Uint64 now = last;
    SDL_Event e;
    _isRunning = true;
    while (_isRunning) {
        last = now;
        now = SDL_GetTicks();
        if (next_game_step >= now) {
            SDL_Delay(next_game_step - now);
            continue;
        }

        while (next_game_step <= now) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    _isRunning = true;
                }
                ImGui_ImplSDL3_ProcessEvent(&e);
                // TODO: Clean this up
                // camera.processSDLEvent(e);
                // camera.update();
                _renderer.set_camera_view(_state.camera.get_view_matrix());
            }

            next_game_step += TIMESTEP_MS;
        }

        auto cmd = _renderer.begin_frame();
        _renderer.draw_game(cmd);
        _renderer.end_frame(cmd, now - last);
    }
}
