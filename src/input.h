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
    void reset();

    glm::vec2 mousePos() { return _mousePos; };
    float scrollDelta() { return _scrollDelta; };
    bool hasPendingPickRequest() { return _hasPendingPickRequest; };
    glm::vec2 lastMousePickPos() { return _lastMousePickPos; };

  private:
    std::array<bool, InputActionType::INPUT_ACTION_TYPE_COUNT> _inputStates;
    glm::vec2 _mousePos{0.0f, 0.0f};
    float _scrollDelta{0.0f};
    bool _hasPendingPickRequest{false};
    glm::vec2 _lastMousePickPos{0.0f, 0.0f};

    void process_left_click(glm::vec2 &&clickPos);
};
