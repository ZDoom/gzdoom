//
// Project: GViZDoom
// File: SDLActionMapper.cpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#include "SDLActionMapper.hpp"


using namespace gvizdoom;


void SDLActionMapper::reset()
{
    _action = Action();
}

void SDLActionMapper::addEvent(const SDL_Event& event)
{
    // TODO proper event handling
    switch (event.type) {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
                case SDLK_w: // forward
                    _action.set(Action::ACTION_FORWARD);
                    break;

                default:
                    break;
            }
            break;

        case SDL_KEYUP:
            switch (event.key.keysym.sym) {
                case SDLK_w: // forward
                    _action.unSet(Action::ACTION_FORWARD);
                    break;

                default:
                    break;
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            switch (event.button.button) {
                case SDL_BUTTON_LEFT: // forward
                    _action.set(Action::ACTION_ATTACK);
                    break;

                default:
                    break;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            switch (event.button.button) {
                case SDL_BUTTON_LEFT: // forward
                    _action.unSet(Action::ACTION_ATTACK);
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

SDLActionMapper::operator const Action&() const
{
    return _action;
}
