#pragma once
#include "camera.h"
#include "input.h"
#include "math/intersection.h"
#include "memory.h"
#include "renderer/renderer.hpp"
#include "tile.h"
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <unordered_map>

// TODO: This value is completely arbitrary
#define GAME_MEMORY 1024 * 1024 * 64

#define SCREEN_WIDTH 960.
#define SCREEN_HEIGHT 540.
#define PLAYER_HEIGHT 1.80
#define PLAYER_WIDTH (0.70 * PLAYER_HEIGHT)
#define PLAYER_SPEED 5.0f

enum UnitType {
    kCube,
};

class Game {
  public:
    Game();
    void init();
    void deinit();
    void run();

  private:
    bool _isRunning{false};
    Arena _arena;
    World *_world;
    entt::registry _registry;
    InputManager _input;

    Camera _camera;
    SDL_Window *_window;
    Renderer _renderer;
    std::shared_ptr<Scene> _assets;

    void update_positions(float dt);

    void handle_pick_request();
    void handle_move_request();
    void render_entities();
    void init_test_entities();

    void add_entity(UnitType &&type, WorldPosition &&pos);

    Ray screen_point_to_ray(glm::vec2 &&point);
};

struct TargetPositionComponent {
    glm::vec3 value;
};

struct MovementSpeed {
    float value;
};

struct UnitData {
    std::string name;
    float movementSpeed;

    static const std::unordered_map<UnitType, UnitData> registry;
};

struct Selected {};
