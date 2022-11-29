//
// Project: GViZDoom
// File: SDLActionMapper.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once


#include <SDL_events.h>
#include <gvizdoom/Action.hpp>


namespace gvizdoom {

class SDLActionMapper {
public:
    SDLActionMapper() = default;

    void reset();
    void addEvent(const SDL_Event& event);

    operator const Action&() const;

private:
    Action  _action;
};

} // namespace gvizdoom
