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
class GameConfig {
public:
    int     argc; // TODO remove raw CLI parameters, use high-level abstraction
    char**  argv;
    bool    interactive {false};

    // Video parameters
    int     videoWidth      {640};
    int     videoHeight     {480};
    int     videoTrueColor  {true};
};

} // namespace gvizdoom
