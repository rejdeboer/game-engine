#include "camera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

void Camera::update() {
    glm::mat4 cameraRotation = get_rotation_matrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}

void Camera::processSDLEvent(SDL_Event &e) {
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_W) {
            velocity.z = -1;
        }
        if (e.key.key == SDLK_S) {
            velocity.z = 1;
        }
        if (e.key.key == SDLK_A) {
            velocity.x = -1;
        }
        if (e.key.key == SDLK_D) {
            velocity.x = 1;
        }
    }

    if (e.type == SDL_EVENT_KEY_UP) {
        if (e.key.key == SDLK_W) {
            velocity.z = 0;
        }
        if (e.key.key == SDLK_S) {
            velocity.z = 0;
        }
        if (e.key.key == SDLK_A) {
            velocity.x = 0;
        }
        if (e.key.key == SDLK_D) {
            velocity.x = 0;
        }
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        yaw += (float)e.motion.xrel / 200.f;
        pitch -= (float)e.motion.yrel / 200.f;
    }
}
