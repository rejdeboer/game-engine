#pragma once
#include <SDL3/SDL_events.h>
#include <glm/glm.hpp>

class Camera {
  public:
    glm::mat4 get_view_matrix();
    glm::mat4 get_projection_matrix();

    void set_aspect_ratio(float ratio);

    void update();
    void processSDLEvent(SDL_Event &e);

  private:
    glm::vec3 _velocity{0.0f, 0.0f, 0.0f};
    glm::vec3 _position{0.0f, 0.0f, 0.0f};
    float _zoom{10.f};
    float _aspectRatio;

    static constexpr float kAngleX = glm::radians(30.0f); // Tilt angle
    static constexpr float kAngleY = glm::radians(45.0f); // Rotation angle
    static constexpr float kCameraDistane = 100.f;
    // TODO: Turn into constexpr using cpp 20+
    const glm::vec3 kOrigin =
        glm::vec3(kCameraDistane * cos(kAngleY), kCameraDistane *sin(kAngleX),
                  kCameraDistane *sin(kAngleY));
};
