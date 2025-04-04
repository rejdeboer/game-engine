#pragma once
#include <SDL3/SDL_events.h>
#include <array>
#include <glm/glm.hpp>

enum InputActionType {
    PAN_X,
    PAN_Y,
    INPUT_ACTION_TYPE_COUNT,
};

class InputManager {
  public:
    InputManager();
    void process_event(const SDL_Event &event);
    bool is_active(InputActionType type);
    void update();

    glm::vec2 mousePos();

  private:
    std::array<bool, InputActionType::INPUT_ACTION_TYPE_COUNT> _inputStates;
    glm::vec2 _mousePos{0.0f, 0.0f};

    void process_left_click(glm::vec2 &&clickPos);
};
