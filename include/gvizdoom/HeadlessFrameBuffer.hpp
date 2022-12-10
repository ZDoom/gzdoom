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

    bool Lock(bool buffer) override;
    void Unlock() override;
    void Update() override;
    PalEntry *GetPalette() override;
    void GetFlashedPalette(PalEntry pal[256]) override;
    void UpdatePalette() override;
    bool SetGamma(float gamma) override;
    bool SetFlash(PalEntry rgb, int amount) override;
    void GetFlash(PalEntry &rgb, int &amount) override;
    int GetPageCount() override;
    bool IsFullscreen() override;

    // Get pixels in BGRA8888 format
    const uint8_t* getPixels() const;

    void SetVSync(bool vsync) override;
    void ScaleCoordsFromWindow(int16_t &x, int16_t &y) override;

private:
    PalEntry                _sourcePalette[256];
    PalEntry                _activePalette[256];
    uint8_t                 _gammaTable[3][256];
    PalEntry                _flash;
    int                     _flashAmount;
    float                   _gamma;
    bool                    _updatePending;
    bool                    _needPalUpdate;
    bool                    _needGammaUpdate;
    std::vector<uint8_t>    _bgraBuffer; // required for paletted drawing and when width != pitch

    void UpdateColors();
    void convertToBGRA();
    void removePitchOverhead(); // in truecolor mode, remove the possible buffer overhead when width != pitch

    HeadlessFrameBuffer() = default;
};

} // namespace gvizdoom
