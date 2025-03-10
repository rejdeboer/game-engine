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
    _state.player_position = WorldPosition{3, 3, 0.0f, 0.0f};

    void *memory = malloc(GAME_MEMORY);
    arena_init(&_arena, GAME_MEMORY, (uint8_t *)memory);

    _world = generate_world(&_arena);
}

void Game::deinit() {
    _renderer.deinit();
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

void Game::run() {
    bool playerDown = false;
    bool playerUp = false;
    bool playerLeft = false;
    bool playerRight = false;

    Uint64 next_game_step = SDL_GetTicks();

    const uint32_t n_tile_rows = 9;
    const uint32_t n_tile_columns = 17;

    float lower_left_x = 0.0f;
    float lower_left_y = SCREEN_HEIGHT;
    Uint64 nextGameStep = SDL_GetTicks();
    Uint64 last = nextGameStep;
    Uint64 now = last;
    SDL_Event e;
    _isRunning = true;
    while (_isRunning) {
        last = now;
        now = SDL_GetTicks();
        if (nextGameStep >= now) {
            SDL_Delay(nextGameStep - now);
            continue;
        }

        while (nextGameStep <= now) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    _isRunning = true;
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

                ImGui_ImplSDL3_ProcessEvent(&e);
                // TODO: Clean this up
                // camera.processSDLEvent(e);
                // camera.update();
                _renderer.set_camera_view(_state.camera.get_view_matrix());

                float dx = PLAYER_SPEED * TIMESTEP_S;
                float dy = PLAYER_SPEED * TIMESTEP_S;

                float new_y;
                if (playerUp && !playerDown) {
                    new_y = _state.player_position.tile_rel_y + dy;
                } else if (!playerUp && playerDown) {
                    new_y = _state.player_position.tile_rel_y - dy;
                }

                if (new_y) {
                    WorldPosition test_left = _state.player_position;
                    WorldPosition test_right = _state.player_position;
                    test_left.tile_rel_y = new_y;
                    test_right.tile_rel_y = new_y;
                    test_left.tile_rel_x -= PLAYER_WIDTH / 2.0f;
                    test_right.tile_rel_x += PLAYER_WIDTH / 2.0f;
                    if (is_world_point_traversible(_world->tile_map,
                                                   test_left) &&
                        is_world_point_traversible(_world->tile_map,
                                                   test_right)) {
                        _state.player_position.tile_rel_y = new_y;
                        _state.player_position = normalize_world_position(
                            _world->tile_map, _state.player_position);
                    }
                }

                if (playerLeft && !playerRight) {
                    WorldPosition test_x = _state.player_position;
                    test_x.tile_rel_x -= dx + PLAYER_WIDTH / 2.0f;
                    if (is_world_point_traversible(_world->tile_map, test_x)) {
                        _state.player_position.tile_rel_x -= dx;
                        _state.player_position = normalize_world_position(
                            _world->tile_map, _state.player_position);
                    }
                } else if (!playerLeft && playerRight) {
                    WorldPosition test_x = _state.player_position;
                    test_x.tile_rel_x += dx + PLAYER_WIDTH / 2.0f;
                    if (is_world_point_traversible(_world->tile_map, test_x)) {
                        _state.player_position.tile_rel_x += dx;
                        _state.player_position = normalize_world_position(
                            _world->tile_map, _state.player_position);
                    }
                }

                next_game_step += TIMESTEP_MS;
            }

            nextGameStep += TIMESTEP_MS;
        }

        auto cmd = _renderer.begin_frame();
        _renderer.draw_game(cmd);
        _renderer.end_frame(cmd, now - last);
    }
}
