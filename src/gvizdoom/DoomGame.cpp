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
#include "gvizdoom/GameState.hpp"
#include "gvizdoom/gzdoom_main_wrapper.hpp"
#include "v_video.h"


//CVars for HUD config
EXTERN_CVAR(Int, hud_scale);
EXTERN_CVAR(Int, st_scale);
EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Bool, hud_althud);
EXTERN_CVAR(Int, hud_althudscale);


using namespace gvizdoom;


DoomGame& DoomGame::instance()
{
    static DoomGame singletonInstance; // singleton instance
    return singletonInstance;
}

void DoomGame::init(GameConfig&& gameConfig)
{
    _gameConfig = gameConfig;
    init();
}

void DoomGame::init(const GameConfig& gameConfig)
{
    _gameConfig = gameConfig;
    init();
}

void DoomGame::restart()
{
    _doomMain.Cleanup();
    reinit();
}

void DoomGame::restart(const GameConfig& gameConfig)
{
    _gameConfig = gameConfig;
    _doomMain.Cleanup();
    reinit();
}

bool DoomGame::update(const Action& action)
{
    _doomLoop.Iter(_context, _gameState, action);

    // Print periodically or upon interesting changes info of the
    // game state
    // This will turn into our way of getting interesting data out of the
    // doom game. For now it will be just printing.
    // We will get map info, game state changes, rewards, enemies,
    // bullets, health, player position, etc
#if 0
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
#endif

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

DoomGame::DoomGame() :
    _status (-1)
{
}

void DoomGame::init()
{
    if (_status != -1) { // already initialized, restart instead
        restart();
        return;
    }

    // Doom main init
    gzdoom_main_init(_gameConfig.argc, _gameConfig.argv);
    _status = _doomMain.Init(_gameConfig.interactive);
    if (_status != 0) {
        return;
    }

    reinit();
}

void DoomGame::reinit()
{
    // Context init
    _context.frameBuffer = std::make_unique<HeadlessFrameBuffer>(
        _gameConfig.videoWidth, _gameConfig.videoHeight, _gameConfig.videoTrueColor);

    _doomMain.ReInit(_context, _gameConfig);
    // Override cvars with values from gameConfig
    initHUD();
    _doomLoop.Init();
}

void DoomGame::initHUD() const
{
    hud_scale = _gameConfig.hudScale;
    st_scale = _gameConfig.hudScale;

    switch (_gameConfig.hudType) {
        case GameConfig::HUD_STATUSBAR:
            screenblocks = 10;
            hud_althud = false;
            st_scale = _gameConfig.hudScale;
            break;
        case GameConfig::HUD_FLOATING:
            screenblocks = 11;
            hud_althud = false;
            hud_scale = _gameConfig.hudScale;
            break;
        case GameConfig::HUD_ALTERNATIVE:
            screenblocks = 11;
            hud_althud = true;
            hud_althudscale = _gameConfig.hudScale;
            break;
    }
}
