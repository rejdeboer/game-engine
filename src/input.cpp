#include "input.h"

InputManager::InputManager() {
    for (int i = 0; i < InputActionType::INPUT_ACTION_TYPE_COUNT; i++) {
        _inputStates[i] = false;
    }
}
