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
#include <vector>


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

    const uint8_t* getPixels() const;

    //friend class SDLGLVideo;

    virtual void SetVSync(bool vsync);
    virtual void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

private:
    PalEntry SourcePalette[256];
    PalEntry    _activePalette[256];
    uint8_t GammaTable[3][256];
    PalEntry Flash;
    int FlashAmount;
    float Gamma;
    bool UpdatePending;

    bool UsingRenderer;
    bool NeedPalUpdate;
    bool NeedGammaUpdate;
    bool NotPaletted;

    std::vector<uint8_t>    _bgraBuffer; // required for paletted drawing and when width != pitch

    void UpdateColors();
    void ResetSDLRenderer();
    void convertToBGRA();
    void removePitchOverhead(); // in truecolor mode, remove the possible buffer overhead when width != pitch

    HeadlessFrameBuffer() {}
};

} // namespace gvizdoom
