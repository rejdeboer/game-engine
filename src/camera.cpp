#include "camera.h"
#include <algorithm>
#include <cstdio>
#include <glm/gtx/transform.hpp>

Camera::Camera(InputManager &input) : _input(input) {}

void Camera::update(float dt) {
    glm::vec2 mousePos = _input.mousePos();
    if (mousePos.x > _screenWidth - 50.f) {
        _position.x += dt * kPanSpeed;
        _isDirty = true;
    } else if (mousePos.x < 50.f) {
        _position.x -= dt * kPanSpeed;
        _isDirty = true;
    }

    if (mousePos.y > _screenHeight - 50.f) {
        _position.z += dt * kPanSpeed;
        _isDirty = true;
    } else if (mousePos.y < 50.f) {
        _position.z -= dt * kPanSpeed;
        _isDirty = true;
    }

    if (std::abs(_input.scrollDelta()) > 0.001f) {
        _zoom = std::clamp(_zoom - _input.scrollDelta(), kMinZoom, kMaxZoom);
        _isDirty = true;
    }
}

// TODO: Can this be a constant?
glm::mat4 Camera::get_view_matrix() {
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f); // Y-axis is "up"
    glm::vec3 cameraDir = glm::normalize(glm::vec3(-1.0f, 1.0f, -1.0f));
    glm::vec3 cameraPosition = cameraTarget + cameraDir * kCameraDistance;
    return glm::lookAt(cameraPosition, cameraTarget, upVector);
}

glm::mat4 Camera::get_projection_matrix() {
    float aspectRatio = _screenWidth / _screenHeight;
    float left = -aspectRatio * _zoom + _position.x;
    float right = aspectRatio * _zoom + _position.x;
    float bottom = -_zoom + _position.z;
    float top = _zoom + _position.z;
    glm::mat4 proj = glm::ortho(left, right, bottom, top, 0.1f, 1000.f);
    proj[1][1] *= -1; // invert Y direction
    return proj;
}

void Camera::set_screen_dimensions(float width, float height) {
    _screenWidth = width;
    _screenHeight = height;
}
