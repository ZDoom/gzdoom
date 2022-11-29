//
// Project: GViZDoom
// File: Action.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once


#include <cstdint>


namespace gvizdoom {


// Action class encapsulates all information required for a game state update
// Actions must be transated to GZDoom actions here or in d_main.cpp or somewhere else
// See: c_buttons.*, d_buttons.* (*=cpp/hpp/h)

// TODO: we cannot use bits to communicate turning unless we do some stupid hack for turn speed
// Options: 
// - set separate bits for different turn speeds, have several
// - change `enum Key` to `struct Foo` that has boolean and integer fields
// to address the fact that some buttons are on/off, some have a signed value
class Action {
public:
    enum Key : uint32_t {
        ACTION_NONE = 0,
        ACTION_FORWARD = 1 << 0,
        ACTION_ATTACK = 1 << 1,
    };

    Action();
    Action(const Action&) = default;
    Action(Action&&) = default;
    Action& operator=(const Action&) = default;
    Action& operator=(Action&&) = default;

    Action(int action, int angle);
    Action(Key key, int angle);

    void set(Key key);
    void unSet(Key key);

    bool isSet(Key key) const;

    int angle(void) const;
    int setAngle(int angle);
private:
    Key _action;
    int _angle;
};

} // namespace gvizdoom
