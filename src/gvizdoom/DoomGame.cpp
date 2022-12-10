//
// Project: GViZDoom
// File: DoomGame.cpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#include "gvizdoom/DoomGame.hpp"
#include "gvizdoom/gzdoom_main_wrapper.hpp"
#include "gvizdoom/HeadlessFrameBuffer.hpp"
#include "v_video.h"


using namespace gvizdoom;


DoomGame::DoomGame() :
    _status         (-1)
{
}

DoomGame::~DoomGame()
{
    _doomMain.Cleanup();
}

void DoomGame::init(GameConfig&& gameConfig)
{
    _gameConfig = gameConfig;

    // Context init
    _context.frameBuffer = std::make_unique<HeadlessFrameBuffer>(
        _gameConfig.videoWidth, _gameConfig.videoHeight, _gameConfig.videoTrueColor);

    // Doom init
    gzdoom_main_init(_gameConfig.argc, _gameConfig.argv);
    _status = _doomMain.Init();
    if (_status != 0) {
        _doomMain.Cleanup();
        return;
    }

    _doomMain.ReInit(_context);
    _doomLoop.Init();
}

void DoomGame::restart()
{
    _doomMain.Cleanup();
    _doomMain.ReInit(_context);
    _doomLoop.Init();
}

bool DoomGame::update(const Action& action)
{
    _doomLoop.Iter(_context, _dbgInfo, action);

    // Print periodically or upon interesting changes info of the
    // game state
    // This will turn into our way of getting interesting data out of the
    // doom game. For now it will be just printing.
    // We will get map info, game state changes, rewards, enemies,
    // bullets, health, player position, etc
    {    
        static MainDebugInfo previousDbgInfo;
        static bool firstStatusPrinted = false;

        if ((  (_dbgInfo.gametic % 100 == 0 or not firstStatusPrinted) and
                _dbgInfo.gametic != previousDbgInfo.gametic) or
            _dbgInfo.wantToRestart or
            _dbgInfo.gamestate != previousDbgInfo.gamestate or
            _dbgInfo.gameaction != previousDbgInfo.gameaction or 
            _dbgInfo.paused != previousDbgInfo.paused)
        {

            printf(">> tic: %d, %s, %s, gstate: %s, gaction: %s, %s lnum %d map %s level %s killed %d\n",
                _dbgInfo.gametic,
                _dbgInfo.singletics ? "single tics" : "multi tics",
                _dbgInfo.wantToRestart ? "restart" : "no restart",
                gamestateToStr(_dbgInfo.gamestate).c_str(),
                gameactionToStr(_dbgInfo.gameaction).c_str(),
                _dbgInfo.paused ? "paused" : "running",
                _dbgInfo.levelNum,
                _dbgInfo.mapName.c_str(),
                _dbgInfo.levelName.c_str(),
                _dbgInfo.killedMonsters);
            
            firstStatusPrinted = true;
        }

        previousDbgInfo = _dbgInfo;
    }

    return false;
}

int DoomGame::getStatus() const
{
    return _status;
}

int DoomGame::getScreenWidth() const
{
    if (_context.frameBuffer == nullptr)
        return 0;
    return _context.frameBuffer->GetWidth();
}

int DoomGame::getScreenHeight() const
{
    if (_context.frameBuffer == nullptr)
        return 0;
    return _context.frameBuffer->GetHeight();
}

const uint8_t* DoomGame::getPixelsBGRA() const
{
    if (_context.frameBuffer == nullptr)
        return nullptr;
    return _context.frameBuffer->getPixels();
}

float* DoomGame::getPixelsDepth() const
{
#if 0 // TODO
    if (_canvas == nullptr)
        return nullptr;
    return _canvas->GetDepthPixels();
#endif
    return nullptr;
}
