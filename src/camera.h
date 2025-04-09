#pragma once
#include "input.h"
#include <glm/glm.hpp>

class Camera {
  public:
    Camera(InputManager &input);

    bool _isDirty{false};

    glm::mat4 get_view_matrix();
    glm::mat4 get_projection_matrix();

    void set_screen_dimensions(float width, float height);

    void update(float dt);

  private:
    glm::vec3 _position{0.0f, 0.0f, 0.0f};
    float _zoom{10.f};
    float _screenWidth;
    float _screenHeight;

    InputManager &_input;

    static constexpr float kPanSpeed = 10.f;
    static constexpr float kMaxZoom = 25.f;
    static constexpr float kMinZoom = 5.f;
    static constexpr float kCameraDistance = 100.f;
};
