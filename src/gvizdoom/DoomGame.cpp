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


#if 0 // TODO
const DCanvas* GetCanvas(); // TODO remove once proper way of fetching canvas is implemented
#endif


using namespace gvizdoom;


DoomGame::DoomGame() :
    _status (-1),
    _canvas (nullptr)
{
}

DoomGame::~DoomGame()
{
    _doomMain.Cleanup();
}

void DoomGame::init(GameConfig&& gameConfig)
{
    gzdoom_main_init(gameConfig.argc, gameConfig.argv);
    _status = _doomMain.Init();
    if (_status != 0) {
        _doomMain.Cleanup();
        return;
    }

//    try {
    _doomMain.ReInit();
    _doomLoop.Init();

//        _status = 0;
//    }
//    catch (const CExitEvent& exit)
//    {
//        _status = exit.Reason();
//        return;
//    }
//    catch (const std::exception& error) {
//        I_ShowFatalError(error.what());
//        _status = -1;
//        return;
//    }

    // Choose between human player and AI player
    //_state.singletics = gameConfig.interactive;
}

void DoomGame::restart()
{
    _doomMain.Cleanup();
    _doomMain.ReInit();
    _doomLoop.Init();

#if 0 // TODO
    // TODO does this do anything? should the whole function be removed?
    try {
        D_DoomMain_Internal_Cleanup();
        D_DoomMain_Internal_ReInit(_state);
    }
    catch (const CExitEvent& exit)
    {
        _status = exit.Reason();
    }
    catch (const std::exception& error) {
        D_DoomMain_Internal_Cleanup();
        I_ShowFatalError(error.what());
        _status = -1;
    }
#endif
}

bool DoomGame::update(const Action& action)
{
#if 0 // TODO
    try {
        // run one cycle
        DoomLoopCycle(_state, action);
    }
    catch (const CExitEvent& exit) // This is a regular exit initiated from deeply nested code.
    {
        _status = exit.Reason();
        return true;
    }
    catch (const std::exception& error) {
        I_ShowFatalError(error.what());
        _status = -1;
    }
#endif

    _doomLoop.Iter();

    return false;
}

int DoomGame::getStatus() const
{
    return _status;
}

int DoomGame::getScreenWidth() const
{
    if (_canvas == nullptr)
        return 0;
    return _canvas->GetWidth();
}

int DoomGame::getScreenHeight() const
{
    if (_canvas == nullptr)
        return 0;
    return _canvas->GetHeight();
}

uint8_t* DoomGame::getPixelsRGBA() const
{
#if 0 // TODO
    if (_canvas == nullptr)
        return nullptr;
    return _canvas->GetPixels();
#endif
    return nullptr;
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

void DoomGame::updateCanvas()
{
#if 0 // TODO
    if (_canvas == nullptr)
        _canvas = GetCanvas();
#endif
}
