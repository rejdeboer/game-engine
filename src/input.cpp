#include "input.h"

InputManager::InputManager() {
    for (int i = 0; i < InputActionType::INPUT_ACTION_TYPE_COUNT; i++) {
        _inputStates[i] = false;
    }
}

void InputManager::process_event(SDL_Event event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
        switch (event.key.key) {
        case SDLK_UP:
        case SDLK_W: {
            _inputStates[InputActionType::PAN_Y] = true;
        } // END W SWITCH
        } // END KEY SWITCH
    } // END DOWN SWITCH
    case SDL_EVENT_KEY_UP: {
        switch (event.key.key) {
        case SDLK_UP:
        case SDLK_W: {
            _inputStates[InputActionType::PAN_Y] = false;
        } // END W SWITCH
        } // END KEY SWITCH
    } // END UP SWITCH
    } // END TYPE SWITCH
}

void InputManager::update() { SDL_GetMouseState(&_mousePos.x, &_mousePos.y); }

glm::vec2 InputManager::mousePos() { return _mousePos; }
