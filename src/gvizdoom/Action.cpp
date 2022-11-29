//
// Project: GViZDoom
// File: Action.cpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#include "gvizdoom/Action.hpp"


using namespace gvizdoom;


Action::Action() :
    _action (ACTION_NONE),
    _angle (0)
{
}

Action::Action(int action, int angle) :
    _action (static_cast<Key>(action)),
    _angle (angle)
{
}

Action::Action(Action::Key key, int angle) :
    _action (key),
    _angle (angle)
{
}

void Action::set(Action::Key key)
{
    _action = static_cast<Key>(static_cast<uint32_t>(_action) | static_cast<uint32_t>(key));
}

void Action::unSet(Action::Key key)
{
    _action = static_cast<Key>(static_cast<uint32_t>(_action) & ~static_cast<uint32_t>(key));
}

bool Action::isSet(Action::Key key) const
{
    return (_action & key);
}

int Action::angle() const
{
    return _angle;
}

int Action::setAngle(int angle)
{
    _angle = angle;
}