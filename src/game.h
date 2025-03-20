#pragma once
#include "camera.h"
#include "memory.h"
#include "renderer/renderer.hpp"
#include "tile.h"
#include <SDL3/SDL.h>
#include <entt/entt.hpp>

// TODO: This value is completely arbitrary
#define GAME_MEMORY 1024 * 1024 * 64

#define SCREEN_WIDTH 960.
#define SCREEN_HEIGHT 540.
#define PLAYER_HEIGHT 1.80
#define PLAYER_WIDTH (0.70 * PLAYER_HEIGHT)
#define PLAYER_SPEED 5.0f

class Game {
  public:
    struct State {
        WorldPosition player_position;
        Camera camera;
    };

    void init();
    void deinit();
    void run();

  private:
    bool _isRunning{false};
    State _state;
    Arena _arena;
    World *_world;
    entt::registry _registry;

    SDL_Window *_window;
    Renderer _renderer;

    void init_test_entities();
};

struct UnitData {
    std::string name;
};

enum UnitType {
    kCube,
};
