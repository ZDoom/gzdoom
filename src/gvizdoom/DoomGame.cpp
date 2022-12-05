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
#include "v_video.h"


extern DFrameBuffer *screen;


using namespace gvizdoom;


DoomGame::DoomGame() :
    _status         (-1),
    _frameBuffer    (nullptr)
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

    _doomMain.ReInit();
    _doomLoop.Init();
    if (screen) {
        _frameBuffer = std::make_unique<HeadlessFrameBuffer>(screen->GetWidth(), screen->GetHeight(), true);
    }
    else {
        printf("Omg no screen!\n");
    }
}

void DoomGame::restart()
{
    _doomMain.Cleanup();
    _doomMain.ReInit();
    _doomLoop.Init();
}

bool DoomGame::update(const Action& action)
{
    _doomLoop.Iter(_frameBuffer.get());

    return false;
}

int DoomGame::getStatus() const
{
    return _status;
}

int DoomGame::getScreenWidth() const
{
    if (_frameBuffer == nullptr)
        return 0;
    return _frameBuffer->GetWidth();
}

int DoomGame::getScreenHeight() const
{
    if (_frameBuffer == nullptr)
        return 0;
    return _frameBuffer->GetHeight();
}

uint8_t* DoomGame::getPixelsRGBA() const
{
    if (_frameBuffer == nullptr)
        return nullptr;
    return _frameBuffer->getMemBuffer();
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
