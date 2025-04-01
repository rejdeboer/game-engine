#include "camera.h"

#include <glm/gtx/transform.hpp>

void Camera::update() { _position += _velocity; }

void Camera::processSDLEvent(SDL_Event &e) {
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_W) {
            _velocity.z = -1;
        }
        if (e.key.key == SDLK_S) {
            _velocity.z = 1;
        }
        if (e.key.key == SDLK_A) {
            _velocity.x = -1;
        }
        if (e.key.key == SDLK_D) {
            _velocity.x = 1;
        }
    }

    if (e.type == SDL_EVENT_KEY_UP) {
        if (e.key.key == SDLK_W) {
            _velocity.z = 0;
        }
        if (e.key.key == SDLK_S) {
            _velocity.z = 0;
        }
        if (e.key.key == SDLK_A) {
            _velocity.x = 0;
        }
        if (e.key.key == SDLK_D) {
            _velocity.x = 0;
        }
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
    }
}

glm::mat4 Camera::get_view_matrix() {
    glm::vec3 cameraPosition = kOrigin + _position;
    glm::vec3 cameraTarget = glm::vec3(_position.x, 0.0f, _position.z);
    glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f); // Y-axis is "up"
    return glm::lookAt(cameraPosition, cameraTarget, upVector);
}

glm::mat4 Camera::get_projection_matrix() {
    float left = -_aspectRatio * _zoom; // Left boundary of the view
    float right = _aspectRatio * _zoom; // Right boundary of the view
    float bottom = -_zoom;              // Bottom boundary of the view
    float top = _zoom;                  // Top boundary of the view
    glm::mat4 proj = glm::ortho(left, right, bottom, top, 0.1f, 1000.f);
    proj[1][1] *= -1; // invert Y direction
    return proj;
}

void Camera::set_aspect_ratio(float ratio) { _aspectRatio = ratio; }
