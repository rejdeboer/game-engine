#pragma once
#include <array>

enum InputActionType {
    PAN_X,
    PAN_Y,
    INPUT_ACTION_TYPE_COUNT,
};

class InputManager {
  public:
    InputManager();
    void update();
    bool is_active(InputActionType type);

  private:
    std::array<bool, InputActionType::INPUT_ACTION_TYPE_COUNT> _inputStates;
};
