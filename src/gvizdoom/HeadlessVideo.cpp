//
// Project: GViZDoom
// File: HeadlessVideo.cpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#include "gvizdoom/HeadlessVideo.hpp"
#include "videomodes.h"


using namespace gvizdoom;


HeadlessVideo::HeadlessVideo (int parm)
{
    IteratorBits = 0;
#if 0
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        fprintf( stderr, "Video initialization failed: %s\n",
             SDL_GetError( ) );
    }
#endif
}

HeadlessVideo::~HeadlessVideo ()
{
}

void HeadlessVideo::StartModeIterator (int bits, bool fs)
{
    IteratorMode = 0;
    IteratorBits = bits;
}

bool HeadlessVideo::NextMode (int *width, int *height, bool *letterbox)
{
    if (IteratorBits != 8)
        return false;

    if ((unsigned)IteratorMode < sizeof(VideoModes)/sizeof(VideoModes[0]))
    {
        *width = VideoModes[IteratorMode].width;
        *height = VideoModes[IteratorMode].height;
        ++IteratorMode;
        return true;
    }
    return false;
}

DFrameBuffer *HeadlessVideo::CreateFrameBuffer (int width, int height, bool bgra, bool fullscreen, DFrameBuffer *old)
{
    // no support for recreating framebuffers in runtime
    if (old == nullptr) {
        throw std::runtime_error("Trying to recreate framebuffer with HeadlessVideo");
    }

    return old;
}

void HeadlessVideo::SetWindowedScale (float scale)
{
}
