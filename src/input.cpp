#include "input.h"

InputManager::InputManager() {
    for (int i = 0; i < InputActionType::INPUT_ACTION_TYPE_COUNT; i++) {
        _inputStates[i] = false;
    }
}

void InputManager::update() { SDL_GetMouseState(&_mousePos.x, &_mousePos.y); }

void InputManager::reset() {
    _scrollDelta = 0.0f;
    _hasPendingPickRequest = false;
}

void InputManager::process_event(const SDL_Event &event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
        switch (event.key.key) {
        case SDLK_UP:
        case SDLK_W: {
            _inputStates[InputActionType::PAN_Y] = true;
            break;
        } // END W SWITCH
        } // END KEY SWITCH
    } // END DOWN SWITCH
    case SDL_EVENT_KEY_UP: {
        switch (event.key.key) {
        case SDLK_UP:
        case SDLK_W: {
            _inputStates[InputActionType::PAN_Y] = false;
            break;
        } // END W SWITCH
        } // END KEY SWITCH
    } // END UP SWITCH
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        switch (event.button.button) {
        case SDL_BUTTON_LEFT: {
            process_left_click({(float)event.button.x, (float)event.button.y});
            break;
        }
        }
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        _scrollDelta += event.wheel.y;
        break;
    }
    } // END TYPE SWITCH
}

void InputManager::process_left_click(glm::vec2 &&clickPos) {
    _hasPendingPickRequest = true;
    _lastMousePickPos = clickPos;
}
