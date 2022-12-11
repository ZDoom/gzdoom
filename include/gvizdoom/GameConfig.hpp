//
// Project: GViZDoom
// File: GameConfig.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once


namespace gvizdoom {

// GameConfig class encapsulates all information required for DoomGame initialization
struct GameConfig {
    int     argc            {0}; // TODO remove raw CLI parameters, use high-level abstraction
    char**  argv            {nullptr};
    bool    interactive     {false};

    // Video parameters
    int     videoWidth      {640};
    int     videoHeight     {480};
    int     videoTrueColor  {true};

    // HUD parameters
    enum HUDType : uint32_t {
        HUD_STATUSBAR = 0, // original Doom HUD with mugshot and grey bar
        HUD_FLOATING = 1, // more minimal HUD with floating icons and ammo/health/other amounts
        HUD_ALTERNATIVE = 2 // alternative floating HUD with more info
    }       hudType         {HUD_STATUSBAR};
    int     hudScale        {2};

    // Game parameters
    int     skill           {3};
    int     episode         {1};
    int     map             {1};
};

} // namespace gvizdoom
