
// HEADER FILES ------------------------------------------------------------

#include "doomtype.h"

#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"

#include "sdlvideo.h"

#include <SDL/SDL.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class SDLFB : public DFrameBuffer
{
public:
	SDLFB (int width, int height, bool fullscreen);
	~SDLFB ();

	bool Lock (bool buffer);
	bool Relock ();
	void ForceBuffering (bool force);
	bool IsValid ();
	void PartialUpdate (int x, int y, int width, int height);
	void Update ();
	PalEntry *GetPalette ();
	void UpdatePalette ();
	bool SetGamma (float gamma);
	bool SetFlash (PalEntry rgb, int amount);
	int GetPageCount ();

	friend class SDLVideo;

private:
	PalEntry SourcePalette[256];
	BYTE GammaTable[256];
	PalEntry Flash;
	int FlashAmount;
	float Gamma;
	
	SDL_Surface *Screen;
	
	bool NeedPalUpdate;
	bool NeedGammaUpdate;
	
	void UpdateColors ();
};

struct MiniModeInfo
{
	WORD Width, Height;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Dummy screen sizes to pass when windowed
static MiniModeInfo WinModes[] =
{
	{ 320, 200 },
	{ 320, 240 },
	{ 400, 300 },
	{ 480, 360 },
	{ 512, 384 },
	{ 640, 400 },
	{ 640, 480 },
	{ 720, 540 },
	{ 800, 600 },
	{ 960, 720 },
	{ 1024, 768 },
	{ 1152, 864 },
	{ 1280, 960 }
};

// CODE --------------------------------------------------------------------

SDLVideo::SDLVideo (int parm)
{
	IteratorBits = 0;
	IteratorFS = false;
}

SDLVideo::~SDLVideo ()
{
}

// This only changes how the iterator lists modes
bool SDLVideo::FullscreenChanged (bool fs)
{
	if (IteratorFS != fs)
	{
		IteratorFS = fs;
		return true;
	}
	return false;
}

int SDLVideo::GetModeCount ()
{
	if (IteratorFS)
	{
		SDL_Rect **modes = SDL_ListModes (NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
		if (modes == NULL)
		{
			return 0;
		}
		if (modes == (SDL_Rect **)-1)
		{
			IteratorFS = 0;
			return sizeof(WinModes)/sizeof(WinModes[0]);
		}
		int count;
		for (count = 0; modes[count]; ++count)
		{
		}
		return count;
	}
	else
	{
		return sizeof(WinModes)/sizeof(WinModes[0]);
	}
}

void SDLVideo::StartModeIterator (int bits)
{
	IteratorMode = 0;
	IteratorBits = bits;
}

bool SDLVideo::NextMode (int *width, int *height)
{
	if (IteratorBits != 8)
		return false;
	
	if (!IteratorFS)
	{
		if (IteratorMode < sizeof(WinModes)/sizeof(WinModes[0]))
		{
			*width = WinModes[IteratorMode].Width;
			*height = WinModes[IteratorMode].Height;
			++IteratorMode;
			return true;
		}
	}
	else
	{
		SDL_Rect **modes = SDL_ListModes (NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
		if (modes != NULL && modes[IteratorMode] != NULL)
		{
			*width = modes[IteratorMode]->w;
			*height = modes[IteratorMode]->h;
			++IteratorMode;
			return true;
		}
	}
	return false;
}

DFrameBuffer *SDLVideo::CreateFrameBuffer (int width, int height, bool fullscreen, DFrameBuffer *old)
{
	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		SDLFB *fb = static_cast<SDLFB *> (old);
		if (fb->Width == width &&
			fb->Height == height)
		{
			bool fsnow = (fb->Screen->flags & SDL_FULLSCREEN) != 0;
	
			if (fsnow != fullscreen)
			{
				SDL_WM_ToggleFullScreen (fb->Screen);
			}
			return old;
		}
		delete old;
	}
	SDLFB *fb = new SDLFB (width, height, fullscreen);
	if (!fb->IsValid ())
	{
		delete fb;
		fb = NULL;
		I_FatalError ("Could not create new screen (%d x %d)\n%s",
			width, height, SDL_GetError());
	}
	return fb;
}

void SDLVideo::SetWindowedScale (float scale)
{
}

// FrameBuffer implementation -----------------------------------------------

SDLFB::SDLFB (int width, int height, bool fullscreen)
	: DFrameBuffer (width, height)
{
	int i;
	
	NeedPalUpdate = false;
	NeedGammaUpdate = false;
	
	Screen = SDL_SetVideoMode (width, height, 8,
		SDL_HWSURFACE|SDL_HWPALETTE|SDL_DOUBLEBUF|
		(fullscreen ? SDL_FULLSCREEN : 0));

	if (Screen == NULL)
		return;

	for (i = 0; i < 256; i++)
	{
		GammaTable[i] = i;
	}
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdateColors ();
}

SDLFB::~SDLFB ()
{
}

bool SDLFB::IsValid ()
{
	return DFrameBuffer::IsValid() && Screen != NULL;
}

int SDLFB::GetPageCount ()
{
	return 1;
}

bool SDLFB::Lock (bool buffered)
{
	return DSimpleCanvas::Lock ();
}

bool SDLFB::Relock ()
{
	return DSimpleCanvas::Lock ();
}

void SDLFB::PartialUpdate (int x, int y, int width, int height)
{
}

void SDLFB::Update ()
{
	Unlock ();

	if (SDL_LockSurface (Screen) == -1)
		return;

	if (Screen->pitch == Pitch)
	{
		memcpy (Screen->pixels, MemBuffer, Width*Height);
	}
	else
	{
		for (int y = 0; y < Height; ++y)
		{
			memcpy ((BYTE *)Screen->pixels+y*Screen->pitch, MemBuffer+y*Width, Width);
		}
	}
	
	SDL_UnlockSurface (Screen);
	SDL_Flip (Screen);

	if (NeedGammaUpdate)
	{
		NeedGammaUpdate = false;
		CalcGamma (Gamma, GammaTable);
		NeedPalUpdate = true;
	}
	
	if (NeedPalUpdate)
	{
		NeedPalUpdate = false;
		UpdateColors ();
	}
}

void SDLFB::UpdateColors ()
{
	SDL_Color colors[256];
		
	for (int i = 0; i < 256; ++i)
	{
		colors[i].r = GammaTable[SourcePalette[i].r];
		colors[i].g = GammaTable[SourcePalette[i].g];
		colors[i].b = GammaTable[SourcePalette[i].b];
	}
	SDL_SetPalette (Screen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);
}

PalEntry *SDLFB::GetPalette ()
{
	return SourcePalette;
}

void SDLFB::UpdatePalette ()
{
	NeedPalUpdate = true;
}

bool SDLFB::SetGamma (float gamma)
{
	Gamma = gamma;
	NeedGammaUpdate = true;
	return true;
}

bool SDLFB::SetFlash (PalEntry rgb, int amount)
{
	Flash.r = GammaTable[rgb.r];
	Flash.g = GammaTable[rgb.g];
	Flash.b = GammaTable[rgb.b];
	FlashAmount = amount;
	NeedPalUpdate = true;
	return true;
}
