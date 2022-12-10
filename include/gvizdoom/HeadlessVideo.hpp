//
// Project: GViZDoom
// File: HeadlessVideo.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once

#include "hardware.h"
#include "v_video.h"
#include <SDL.h>


EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Color, dimcolor)

//struct FRenderer;
//FRenderer *gl_CreateInterface();

namespace gvizdoom {

class HeadlessVideo : public IVideo
{
public:
    HeadlessVideo (int parm);
    ~HeadlessVideo ();

    EDisplayType GetDisplayType () { return DISPLAY_Both; }
    void SetWindowedScale (float scale);

    DFrameBuffer *CreateFrameBuffer (int width, int height, bool bgra, bool fs, DFrameBuffer *old);

    void StartModeIterator (int bits, bool fs);
    bool NextMode (int *width, int *height, bool *letterbox);
    //bool SetResolution (int width, int height, int bits);

private:
    int IteratorMode;
    int IteratorBits;
};

} // namespace gvizdoom
