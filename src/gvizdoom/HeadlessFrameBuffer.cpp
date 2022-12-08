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

EXTERN_CVAR (Float, rgamma)
EXTERN_CVAR (Float, ggamma)
EXTERN_CVAR (Float, bgamma)


using namespace gvizdoom;


HeadlessFrameBuffer::HeadlessFrameBuffer (int width, int height, bool bgra)
    : DFrameBuffer (width, height, bgra)
{
    int i;

    NeedPalUpdate = false;
    NeedGammaUpdate = false;
    UpdatePending = false;
    NotPaletted = false;
    FlashAmount = 0;

    ResetSDLRenderer ();

    for (i = 0; i < 256; i++)
    {
        GammaTable[0][i] = GammaTable[1][i] = GammaTable[2][i] = i;
    }

    memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
    UpdateColors ();

#ifdef __APPLE__
    SetVSync (vid_vsync);
#endif
}


HeadlessFrameBuffer::~HeadlessFrameBuffer ()
{
#if 0
    if (Renderer)
    {
        if (Texture)
            SDL_DestroyTexture (Texture);
        SDL_DestroyRenderer (Renderer);
    }

    if(Screen)
    {
        SDL_DestroyWindow (Screen);
    }
#endif
}

bool HeadlessFrameBuffer::IsValid ()
{
    return DFrameBuffer::IsValid()/* && Screen != NULL*/;
}

int HeadlessFrameBuffer::GetPageCount ()
{
    return 1;
}

bool HeadlessFrameBuffer::Lock (bool buffered)
{
    return DSimpleCanvas::Lock ();
}

bool HeadlessFrameBuffer::Relock ()
{
    return DSimpleCanvas::Lock ();
}

void HeadlessFrameBuffer::Unlock ()
{
    if (UpdatePending && LockCount == 1)
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
            UpdatePending = true;
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
    UpdatePending = false;
#if 0
    BlitCycles.Reset();
    SDLFlipCycles.Reset();
    BlitCycles.Clock();
#endif
    void *pixels = MemBuffer;
    int pitch = Pitch * 4;
#if 0
    if (UsingRenderer)
    {
        if (SDL_LockTexture (Texture, NULL, &pixels, &pitch))
            return;
    }
    else
    {
        if (SDL_LockSurface (Surface))
            return;

        pixels = Surface->pixels;
        pitch = Surface->pitch;
    }
#endif
#if 1 // TODO maybe needed for gammaslääkäri
    if (Bgra)
    {
        CopyWithGammaBgra(pixels, pitch, GammaTable[0], GammaTable[1], GammaTable[2], Flash, FlashAmount);
    }
    else if (NotPaletted)
    {
        GPfx.Convert (MemBuffer, Pitch,
            pixels, pitch, Width, Height,
            FRACUNIT, FRACUNIT, 0, 0);
    }
    else
    {
        if (pitch == Pitch)
        {
            memcpy (pixels, MemBuffer, Width*Height);
        }
        else
        {
            for (int y = 0; y < Height; ++y)
            {
                memcpy ((uint8_t *)pixels+y*pitch, MemBuffer+y*Pitch, Width);
            }
        }
    }
#endif
#if 0
    if (UsingRenderer)
    {
        SDL_UnlockTexture (Texture);

        SDLFlipCycles.Clock();
        SDL_RenderClear(Renderer);
        SDL_RenderCopy(Renderer, Texture, NULL, NULL);
        SDL_RenderPresent(Renderer);
        SDLFlipCycles.Unclock();
    }
    else
    {
        SDL_UnlockSurface (Surface);

        SDLFlipCycles.Clock();
        SDL_UpdateWindowSurface (Screen);
        SDLFlipCycles.Unclock();
    }
#endif
    //BlitCycles.Unclock();

    if (NeedGammaUpdate)
    {
        bool Windowed = false;
        NeedGammaUpdate = false;
        CalcGamma ((Windowed || rgamma == 0.f) ? Gamma : (Gamma * rgamma), GammaTable[0]);
        CalcGamma ((Windowed || ggamma == 0.f) ? Gamma : (Gamma * ggamma), GammaTable[1]);
        CalcGamma ((Windowed || bgamma == 0.f) ? Gamma : (Gamma * bgamma), GammaTable[2]);
        NeedPalUpdate = true;
    }

    if (NeedPalUpdate)
    {
        NeedPalUpdate = false;
        UpdateColors ();
    }
}

void HeadlessFrameBuffer::UpdateColors ()
{
    //if (NotPaletted)
    {
        PalEntry palette[256];

        for (int i = 0; i < 256; ++i)
        {
            palette[i].r = GammaTable[0][SourcePalette[i].r];
            palette[i].g = GammaTable[1][SourcePalette[i].g];
            palette[i].b = GammaTable[2][SourcePalette[i].b];
        }
        if (FlashAmount)
        {
            DoBlending (palette, palette,
                256, GammaTable[0][Flash.r], GammaTable[1][Flash.g], GammaTable[2][Flash.b],
                FlashAmount);
        }
        GPfx.SetPalette (palette);
    }
#if 0
    else
    {
        SDL_Color colors[256];

        for (int i = 0; i < 256; ++i)
        {
            colors[i].r = GammaTable[0][SourcePalette[i].r];
            colors[i].g = GammaTable[1][SourcePalette[i].g];
            colors[i].b = GammaTable[2][SourcePalette[i].b];
        }
        if (FlashAmount)
        {
            DoBlending ((PalEntry *)colors, (PalEntry *)colors,
                256, GammaTable[2][Flash.b], GammaTable[1][Flash.g], GammaTable[0][Flash.r],
                FlashAmount);
        }
        SDL_SetPaletteColors (Surface->format->palette, colors, 0, 256);
    }
#endif
}

PalEntry *HeadlessFrameBuffer::GetPalette ()
{
    return SourcePalette;
}

void HeadlessFrameBuffer::UpdatePalette ()
{
    NeedPalUpdate = true;
}

bool HeadlessFrameBuffer::SetGamma (float gamma)
{
    Gamma = gamma;
    NeedGammaUpdate = true;
    return true;
}

bool HeadlessFrameBuffer::SetFlash (PalEntry rgb, int amount)
{
    Flash = rgb;
    FlashAmount = amount;
    NeedPalUpdate = true;
    return true;
}

void HeadlessFrameBuffer::GetFlash (PalEntry &rgb, int &amount)
{
    rgb = Flash;
    amount = FlashAmount;
}

// Q: Should I gamma adjust the returned palette?
void HeadlessFrameBuffer::GetFlashedPalette (PalEntry pal[256])
{
    memcpy (pal, SourcePalette, 256*sizeof(PalEntry));
    if (FlashAmount)
    {
        DoBlending (pal, pal, 256, Flash.r, Flash.g, Flash.b, FlashAmount);
    }
}

bool HeadlessFrameBuffer::IsFullscreen ()
{
    return false;
}

void HeadlessFrameBuffer::ResetSDLRenderer ()
{
#if 0
    if (Renderer)
    {
        if (Texture)
            SDL_DestroyTexture (Texture);
        SDL_DestroyRenderer (Renderer);
    }

    UsingRenderer = !vid_forcesurface;
    if (UsingRenderer)
    {
        Renderer = SDL_CreateRenderer (Screen, -1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_TARGETTEXTURE|
            (vid_vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
        if (!Renderer)
            return;

        SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);

        Uint32 fmt;
        if (Bgra)
        {
            fmt = SDL_PIXELFORMAT_ARGB8888;
        }
        else
        {
            switch (vid_displaybits)
            {
                default: fmt = SDL_PIXELFORMAT_ARGB8888; break;
                case 30: fmt = SDL_PIXELFORMAT_ARGB2101010; break;
                case 24: fmt = SDL_PIXELFORMAT_RGB888; break;
                case 16: fmt = SDL_PIXELFORMAT_RGB565; break;
                case 15: fmt = SDL_PIXELFORMAT_ARGB1555; break;
            }
        }
        Texture = SDL_CreateTexture (Renderer, fmt, SDL_TEXTUREACCESS_STREAMING, Width, Height);

        {
            NotPaletted = true;

            Uint32 format;
            SDL_QueryTexture(Texture, &format, NULL, NULL, NULL);

            Uint32 Rmask, Gmask, Bmask, Amask;
            int bpp;
            SDL_PixelFormatEnumToMasks(format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
            GPfx.SetFormat (bpp, Rmask, Gmask, Bmask);
        }
    }
    else
    {
        Surface = SDL_GetWindowSurface (Screen);

        if (Surface->format->palette == NULL)
        {
            NotPaletted = true;
            GPfx.SetFormat (Surface->format->BitsPerPixel, Surface->format->Rmask, Surface->format->Gmask, Surface->format->Bmask);
        }
        else
            NotPaletted = false;
    }

#endif
    GPfx.SetFormat(32, 0xff0000, 0x00ff00, 0x0000ff);
}

void HeadlessFrameBuffer::SetVSync (bool vsync)
{
#if 0 // #ifdef __APPLE__
    if (CGLContextObj context = CGLGetCurrentContext())
	{
		// Apply vsync for native backend only (where OpenGL context is set)
		const GLint value = vsync ? 1 : 0;
		CGLSetParameter(context, kCGLCPSwapInterval, &value);
	}
#else
    ResetSDLRenderer ();
#endif // __APPLE__
}

void HeadlessFrameBuffer::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
{
    int w = Width;
    int h = Height;
    //SDL_GetWindowSize (Screen, &w, &h);

    x = (int16_t)(x*Width/w);
    y = (int16_t)(y*Height/h);
}

