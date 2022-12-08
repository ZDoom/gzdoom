//
// Project: GViZDoom
// File: HeadlessFrameBuffer.hpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#pragma once

#include "v_video.h"


namespace gvizdoom {

class HeadlessFrameBuffer : public DFrameBuffer
{
    typedef DFrameBuffer Super;
public:
    HeadlessFrameBuffer(int width, int height, bool bgra);
    ~HeadlessFrameBuffer();

    bool Lock(bool buffer);
    void Unlock();
    bool Relock();
    void ForceBuffering(bool force);
    bool IsValid();
    void Update();
    PalEntry *GetPalette();
    void GetFlashedPalette(PalEntry pal[256]);
    void UpdatePalette();
    bool SetGamma(float gamma);
    bool SetFlash(PalEntry rgb, int amount);
    void GetFlash(PalEntry &rgb, int &amount);
    int GetPageCount();
    bool IsFullscreen();

    //friend class SDLGLVideo;

    virtual void SetVSync(bool vsync);
    virtual void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

private:
    PalEntry SourcePalette[256];
    uint8_t GammaTable[3][256];
    PalEntry Flash;
    int FlashAmount;
    float Gamma;
    bool UpdatePending;

    bool UsingRenderer;
    bool NeedPalUpdate;
    bool NeedGammaUpdate;
    bool NotPaletted;

    void UpdateColors();
    void ResetSDLRenderer();

    HeadlessFrameBuffer() {}
};

} // namespace gvizdoom
