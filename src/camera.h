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
    static constexpr float kAngleX = glm::radians(30.0f); // Tilt angle
    static constexpr float kAngleY = glm::radians(45.0f); // Rotation angle
    static constexpr float kDistance = 100.f;
    // TODO: Turn into constexpr using cpp 20+
    const glm::vec3 kOrigin =
        glm::vec3(kDistance * cos(kAngleY), kDistance *sin(kAngleX),
                  kDistance *sin(kAngleY));
};
