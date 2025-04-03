#pragma once
#include <SDL3/SDL.h>
#include <array>

enum InputActionType {
    PAN_X,
    PAN_Y,
    INPUT_ACTION_TYPE_COUNT,
};

class InputManager {
  public:
    InputManager();
    void process_event(SDL_Event event);
    bool is_active(InputActionType type);

  private:
    std::array<bool, InputActionType::INPUT_ACTION_TYPE_COUNT> _inputStates;
};
