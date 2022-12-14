//
// Project: GViZDoom
// File: DoomGame.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once


#include "gvizdoom/Action.hpp"
#include "gvizdoom/GameConfig.hpp"
#include "gvizdoom/GameState.hpp"
#include "gvizdoom/Context.hpp"
#include "gvizdoom/HeadlessFrameBuffer.hpp"
#include "d_main.h"


namespace gvizdoom {

// DoomGame serves as the top-level interface for the GViZDoom library
class DoomGame {
public:
    DoomGame(const DoomGame&) = delete;
    DoomGame(DoomGame&&) = delete;
    DoomGame& operator=(const DoomGame&) = delete;
    DoomGame& operator=(DoomGame&&) = delete;

    static DoomGame& instance();

    void init(GameConfig&& gameConfig);
    void init(const GameConfig& gameConfig);
    void restart();
    void restart(const GameConfig& gameConfig);
    bool update(const Action& action); // returns true upon exit
    int getStatus() const;

    int getScreenWidth() const;
    int getScreenHeight() const;
    const uint8_t* getPixelsBGRA() const;
    float* getPixelsDepth() const;

    template <GameState T_GameState>
    const typename GameStateInfo<T_GameState>::Type& getGameState()
    {
        return _gameState.get<T_GameState>();
    }

private:
    DoomGame();

    GameConfig           _gameConfig;
    int                  _status;
    DoomMain             _doomMain;
    DoomLoop             _doomLoop;
    Context              _context;
    GameStateContainer   _gameState;

    void init();
    void reinit();
    void initHUD() const;
};

} // namespace gvizdoom
