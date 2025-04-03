#pragma once
#include <unordered_map>

enum InputActionType {
    PAN_X,
    PAN_Y,
    INPUT_ACTION_TYPE_COUNT,
};

class InputManager {
  public:
    void update();
    bool is_active(InputActionType type);

  private:
    std::unordered_map<InputActionType, bool> _inputStates;
};
