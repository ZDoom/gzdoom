//
// Project: GViZDoom
// File: HeadlessFrameBuffer.cpp
//
// Copyright (c) 2022 Miika 'Lehdari' Lehtimäki
// You may use, distribute and modify this code under the terms
// of the licence specified in file LICENSE which is distributed
// with this source code package.
//

#include "HeadlessFrameBuffer.hpp"
#include "v_palette.h"
#include "hardware.h"
#include "v_pfx.h"


EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_maxfps)
EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, vid_vsync)

CUSTOM_CVAR (Float, rgamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
    if (screen != NULL)
    {
        screen->SetGamma (Gamma);
    }
}
CUSTOM_CVAR (Float, ggamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
    if (screen != NULL)
    {
        screen->SetGamma (Gamma);
    }
}
CUSTOM_CVAR (Float, bgamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
    if (screen != NULL)
    {
        screen->SetGamma (Gamma);
    }
}


using namespace gvizdoom;


HeadlessFrameBuffer::HeadlessFrameBuffer (int width, int height, bool bgra) :
    DFrameBuffer    (width, height, bgra),
    _bgraBuffer     ((!bgra || Pitch != Width) ? 4 * width * height : 0)
{
    int i;

    _needPalUpdate = false;
    _needGammaUpdate = false;
    _updatePending = false;
    _flashAmount = 0;

    for (i = 0; i < 256; i++)
    {
        _gammaTable[0][i] = _gammaTable[1][i] = _gammaTable[2][i] = i;
    }

    memcpy (_sourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
    UpdateColors ();

#ifdef __APPLE__
    SetVSync (vid_vsync);
#endif
}

int HeadlessFrameBuffer::GetPageCount ()
{
    return 1;
}

bool HeadlessFrameBuffer::Lock (bool buffered)
{
    return DSimpleCanvas::Lock ();
}

void HeadlessFrameBuffer::Unlock ()
{
    if (_updatePending && LockCount == 1)
    {
        Update ();
    }
    else if (--LockCount <= 0)
    {
        Buffer = NULL;
        LockCount = 0;
    }
}

void HeadlessFrameBuffer::Update ()
{
    if (LockCount != 1)
    {
        if (LockCount > 0)
        {
            _updatePending = true;
            --LockCount;
        }
        return;
    }

    DrawRateStuff ();

#if !defined(__APPLE__) && !defined(__OpenBSD__)
    if(vid_maxfps && !cl_capfps)
    {
        SEMAPHORE_WAIT(FPSLimitSemaphore)
    }
#endif

    Buffer = NULL;
    LockCount = 0;
    _updatePending = false;

    void *pixels = MemBuffer;
    int pitch = Bgra ? Pitch * 4 : Pitch;

    if (Bgra) {
        CopyWithGammaBgra(pixels, pitch, _gammaTable[0], _gammaTable[1], _gammaTable[2], _flash, _flashAmount);
    }

    if (_needGammaUpdate) {
        bool Windowed = false;
        _needGammaUpdate = false;
        CalcGamma ((Windowed || rgamma == 0.f) ? _gamma : (_gamma * rgamma), _gammaTable[0]);
        CalcGamma ((Windowed || ggamma == 0.f) ? _gamma : (_gamma * ggamma), _gammaTable[1]);
        CalcGamma ((Windowed || bgamma == 0.f) ? _gamma : (_gamma * bgamma), _gammaTable[2]);
        _needPalUpdate = true;
    }

    if (_needPalUpdate) {
        _needPalUpdate = false;
        UpdateColors ();
    }

    if (!Bgra) {
        convertToBGRA();
    }
    else if (Pitch != Width) {
        removePitchOverhead();
    }
}

void HeadlessFrameBuffer::UpdateColors ()
{
    for (int i = 0; i < 256; ++i)
    {
        _activePalette[i].r = _gammaTable[0][_sourcePalette[i].r];
        _activePalette[i].g = _gammaTable[1][_sourcePalette[i].g];
        _activePalette[i].b = _gammaTable[2][_sourcePalette[i].b];
    }
    if (_flashAmount)
    {
        DoBlending (_activePalette, _activePalette,
            256, _gammaTable[0][_flash.r], _gammaTable[1][_flash.g], _gammaTable[2][_flash.b],
            _flashAmount);
    }
}

PalEntry *HeadlessFrameBuffer::GetPalette ()
{
    return _sourcePalette;
}

void HeadlessFrameBuffer::UpdatePalette ()
{
    _needPalUpdate = true;
}

bool HeadlessFrameBuffer::SetGamma (float gamma)
{
    _gamma = gamma;
    _needGammaUpdate = true;
    return true;
}

bool HeadlessFrameBuffer::SetFlash (PalEntry rgb, int amount)
{
    _flash = rgb;
    _flashAmount = amount;
    _needPalUpdate = true;
    return true;
}

void HeadlessFrameBuffer::GetFlash (PalEntry &rgb, int &amount)
{
    rgb = _flash;
    amount = _flashAmount;
}

// Q: Should I gamma adjust the returned palette?
void HeadlessFrameBuffer::GetFlashedPalette (PalEntry pal[256])
{
    memcpy (pal, _sourcePalette, 256*sizeof(PalEntry));
    if (_flashAmount)
    {
        DoBlending (pal, pal, 256, _flash.r, _flash.g, _flash.b, _flashAmount);
    }
}

bool HeadlessFrameBuffer::IsFullscreen ()
{
    return false;
}

const uint8_t* HeadlessFrameBuffer::getPixels() const
{
    if (Bgra && Pitch == Width)
        return MemBuffer;
    else
        return _bgraBuffer.data();
}

void HeadlessFrameBuffer::SetVSync (bool vsync)
{
#ifdef __APPLE__
    if (CGLContextObj context = CGLGetCurrentContext())
	{
		// Apply vsync for native backend only (where OpenGL context is set)
		const GLint value = vsync ? 1 : 0;
		CGLSetParameter(context, kCGLCPSwapInterval, &value);
	}
#endif // __APPLE__
}

void HeadlessFrameBuffer::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
{
    int w = Width;
    int h = Height;

    x = (int16_t)(x*Width/w);
    y = (int16_t)(y*Height/h);
}

void HeadlessFrameBuffer::convertToBGRA()
{
    for (int j=0; j<Height; ++j) {
        auto* row = reinterpret_cast<uint32_t*>(_bgraBuffer.data() + (j * Width)*4);
        for (int i=0; i<Width; ++i) {
            auto& p = _activePalette[MemBuffer[j*Pitch + i]];
            row[i] = p.d;
        }
    }
}

void HeadlessFrameBuffer::removePitchOverhead()
{
    for (int j=0; j<Height; ++j) {
        uint8_t* srcRow = MemBuffer + (j * Pitch) * 4;
        uint8_t* destRow = _bgraBuffer.data() + (j * Width) * 4;
        memcpy(destRow, srcRow, sizeof(uint8_t) * Width * 4);
    }
}
