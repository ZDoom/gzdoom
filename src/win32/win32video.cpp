
// HEADER FILES ------------------------------------------------------------

#ifndef INITGUID
#define INITGUID
#endif

#define DIRECTDRAW_VERSION 0x0300

#include <objbase.h>
#include <initguid.h>
#include <ddraw.h>
#include <stdio.h>

#define __BYTEBOOL__
#include "doomtype.h"

#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"

#include "win32iface.h"

// MACROS ------------------------------------------------------------------

#define true TRUE
#define false FALSE

#if 0
#define STARTLOG		{ if (!dbg) dbg = fopen ("f:/vid.log", "w"); }
#define STOPLOG			{ if (dbg) { fclose (dbg); dbg=NULL; } }
#define LOG(x)			{ if (dbg) { fprintf (dbg, x); fflush (dbg); } }
#define LOG1(x,y)		{ if (dbg) { fprintf (dbg, x, y); fflush (dbg); } }
#define LOG2(x,y,z)		{ if (dbg) { fprintf (dbg, x, y, z); fflush (dbg); } }
#define LOG3(x,y,z,zz)	{ if (dbg) { fprintf (dbg, x, y, z, zz); fflush (dbg); } }
#define LOG4(x,y,z,a,b)	{ if (dbg) { fprintf (dbg, x, y, z, a, b); fflush (dbg); } }
FILE *dbg;
#else
#define STARTLOG
#define STOPLOG
#define LOG(x)
#define LOG1(x,y)
#define LOG2(x,y,z)
#define LOG3(x,y,z,zz)
#define LOG4(x,y,z,a,b)
#endif

// TYPES -------------------------------------------------------------------

class DDrawFB : public DFrameBuffer
{
public:
	DDrawFB (int width, int height, bool fullscreen);
	~DDrawFB ();

	bool IsValid ();
	bool Lock ();
	bool Lock (bool buffer);
	bool Relock ();
	void Unlock ();
	void ForceBuffering (bool force);
	void PartialUpdate (int x, int y, int width, int height);
	void Update ();
	PalEntry *GetPalette ();
	void UpdatePalette ();
	bool SetGamma (float gamma);
	bool SetFlash (PalEntry rgb, int amount);
	void GetFlash (PalEntry &rgb, int &amount);
	int GetPageCount ();
	int QueryNewPalette ();
	HRESULT GetHR () { return LastHR; }
	bool IsFullscreen () { return !Windowed; }

private:
	bool CreateResources ();
	void ReleaseResources ();
	bool CreateSurfacesAttached ();
	bool CreateSurfacesComplex ();
	enum LockSurfRes { NoGood, Good, GoodWasLost } LockSurf (LPRECT lockrect);
	void RebuildColorTable ();
	void CopyAndRemap ();
	void MaybeCreatePalette ();
	bool AddBackBuf (LPDIRECTDRAWSURFACE *surface, int num);

	HRESULT LastHR;
	BYTE GammaTable[256];
	PalEntry SourcePalette[256];
	PALETTEENTRY PalEntries[256];

	LPDIRECTDRAWPALETTE Palette;
	LPDIRECTDRAWSURFACE PrimarySurf;
	LPDIRECTDRAWSURFACE BackSurf;
	LPDIRECTDRAWSURFACE BackSurf2;
	LPDIRECTDRAWSURFACE LockingSurf;
	LPDIRECTDRAWCLIPPER Clipper;
	IDirectDrawGammaControl *GammaControl;
	HPALETTE GDIPalette;
	BYTE *ClipRegion;
	DWORD ClipSize;
	PalEntry Flash;
	int FlashAmount;
	int BufferCount;
	float Gamma;

	bool NeedGammaUpdate;
	bool NeedPalUpdate;
	bool MustBuffer;		// The screen is not 8-bit, or there is no backbuffer
	bool Windowed;
	bool BufferingNow;		// Most recent Lock was buffered
	bool WasBuffering;		// Second most recent Lock was buffered
	bool Write8bit;
	bool UpdatePending;		// On final unlock, call Update()

	friend class Win32Video;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern IVideo *Video;
extern BOOL AppActive;
extern bool FullscreenReset;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool, vid_palettehack, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, vid_attachedsurfaces, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static IDirectDraw2 *DDraw;

// CODE --------------------------------------------------------------------

Win32Video::Win32Video (int parm)
: m_CalledCoInitialize (false),
  m_Modes (NULL),
  m_Have320x200x8 (false),
  m_Have320x240x8 (false),
  m_IsFullscreen (false)
{
	STARTLOG

	CBData cbdata;
	HRESULT dderr;

	CoInitialize (NULL);
	m_CalledCoInitialize = true;

	dderr = CoCreateInstance (CLSID_DirectDraw, 0, CLSCTX_ALL, IID_IDirectDraw2, (void **)&DDraw);

	if (FAILED(dderr))
		I_FatalError ("Could not create DirectDraw object: %08x", dderr);

	dderr = DDraw->Initialize (0);
	if (FAILED(dderr))
	{
		DDraw->Release ();
		DDraw = NULL;
		I_FatalError ("Could not initialize IDirectDraw2 interface: %08x", dderr);
	}

	DDraw->SetCooperativeLevel (Window, DDSCL_NORMAL);
	FreeModes ();
	cbdata.self = this;
	cbdata.modes = (ModeInfo *)&m_Modes;
	dderr = DDraw->EnumDisplayModes (0, NULL, &cbdata, EnumDDModesCB);
	if (FAILED(dderr))
	{
		DDraw->Release ();
		DDraw = NULL;
		I_FatalError ("Could not enumerate display modes: %08x", dderr);
	}
	if (m_Modes == NULL)
	{
		DDraw->Release ();
		DDraw = NULL;
		I_FatalError ("DirectDraw returned no display modes.\n\n"
					  "If you started ZDoom from a fullscreen DOS box, run it from "
					  "a DOS window instead. If that does not work, you may need to reboot.");
	}
	if (OSPlatform == os_Win95)
	{
		// Windows 95 will let us use Mode X. If we didn't find any linear
		// modes in the loop above, add the Mode X modes here.

		if (!m_Have320x200x8)
			AddMode (320, 200, 8, &cbdata.modes);
		if (!m_Have320x240x8)
			AddMode (320, 240, 8, &cbdata.modes);
	}
}

Win32Video::~Win32Video ()
{
	FreeModes ();

	if (DDraw != NULL)
	{
		if (m_IsFullscreen)
		{
			DDraw->SetCooperativeLevel (NULL, DDSCL_NORMAL);
		}
		DDraw->Release();
		DDraw = NULL;
	}

	if (m_CalledCoInitialize)
	{
		CoUninitialize ();
		m_CalledCoInitialize = false;
	}

	ShowWindow (Window, SW_HIDE);

	STOPLOG
}

// Returns true if fullscreen, false otherwise
bool Win32Video::GoFullscreen (bool yes)
{
	static const char *const yestypes[2] = { "windowed", "fullscreen" };
	HRESULT hr[2];
	int count;

	if (m_IsFullscreen == yes)
		return yes;

	for (count = 0; count < 2; ++count)
	{
		LOG1 ("fullscreen: %d\n", yes);
		hr[count] = DDraw->SetCooperativeLevel (Window, !yes ? DDSCL_NORMAL :
			DDSCL_ALLOWMODEX | DDSCL_ALLOWREBOOT | DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
		if (SUCCEEDED(hr[count]))
		{
			if (count != 0)
			{
// Ack! Cannot print because the screen does not exist right now.
//				Printf ("Setting %s mode failed. Error %08x\n",
//					yestypes[!yes], hr[0]);
			}
			m_IsFullscreen = yes;
			return yes;
		}
		yes = !yes;
	}

	I_FatalError ("Could not set %s mode: %08x\n",
				  "Could not set %s mode: %08x\n",
				  yestypes[yes], hr[0], yestypes[!yes], hr[1]);

	// Appease the compiler, even though we never return if we get here.
	return false;
}

// Mode enumeration --------------------------------------------------------

HRESULT WINAPI Win32Video::EnumDDModesCB (LPDDSURFACEDESC desc, void *data)
{
	if (desc->ddpfPixelFormat.dwRGBBitCount == 8 &&
		desc->dwHeight <= 1024)
	{
		((CBData *)data)->self->AddMode
			(desc->dwWidth, desc->dwHeight, 8,
			&((CBData *)data)->modes);
	}

	return DDENUMRET_OK;
}

void Win32Video::AddMode (int x, int y, int bits, ModeInfo **lastmode)
{
	ModeInfo *newmode = new ModeInfo (x, y, bits);

	if (x == 320 && bits == 8)
	{
		if (y == 200)
			m_Have320x200x8 = true;
		else if (y == 240)
			m_Have320x240x8 = true;
	}
	(*lastmode)->next = newmode;
	*lastmode = newmode;
}

void Win32Video::FreeModes ()
{
	ModeInfo *mode = m_Modes;

	while (mode)
	{
		ModeInfo *tempmode = mode;
		mode = mode->next;
		delete tempmode;
	}
	m_Modes = NULL;
}

// This only changes how the iterator lists modes
bool Win32Video::FullscreenChanged (bool fs)
{
	LOG1 ("FS-changed: %d\n", fs);
	m_IteratorFS = fs;
	return true;
}

int Win32Video::GetModeCount ()
{
	int count = 0;
	ModeInfo *mode = m_Modes;

	while (mode)
	{
		count++;
		mode = mode->next;
	}
	return count;
}

void Win32Video::StartModeIterator (int bits)
{
	m_IteratorMode = m_Modes;
	m_IteratorBits = bits;
}

bool Win32Video::NextMode (int *width, int *height)
{
	if (m_IteratorMode)
	{
		while (m_IteratorMode && m_IteratorMode->bits != m_IteratorBits)
			m_IteratorMode = m_IteratorMode->next;

		if (m_IteratorMode)
		{
			*width = m_IteratorMode->width;
			*height = m_IteratorMode->height;
			m_IteratorMode = m_IteratorMode->next;
			return true;
		}
	}
	return false;
}

DFrameBuffer *Win32Video::CreateFrameBuffer (int width, int height, bool fullscreen, DFrameBuffer *old)
{
	static int retry = 0;
	static int owidth, oheight;

	PalEntry flashColor;
	int flashAmount;

	LOG4 ("CreateFB %d %d %d %p\n", width, height, fullscreen, old);

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		DDrawFB *fb = static_cast<DDrawFB *> (old);
		if (fb->Width == width &&
			fb->Height == height &&
			fb->Windowed == !fullscreen)
		{
			return old;
		}
		old->GetFlash (flashColor, flashAmount);
		delete old;
	}
	else
	{
		flashColor = 0;
		flashAmount = 0;
	}

	DDrawFB *fb = new DDrawFB (width, height, fullscreen);
	LOG1 ("New fb created @ %p\n", fb);

	retry = 0;

	// If we could not create the framebuffer, try again with slightly
	// different parameters in this order:
	// 1. Try with the closest size
	// 2. Try in the opposite screen mode with the original size
	// 3. Try in the opposite screen mode with the closest size
	// This is a somewhat confusing mass of recursion here.

	while (fb == NULL || !fb->IsValid ())
	{
		static HRESULT hr;

		if (fb != NULL)
		{
			if (retry == 0)
			{
				hr = fb->GetHR ();
			}
			delete fb;

			LOG1 ("fb is bad: %08x\n", hr);
		}
		else
		{
			LOG ("Could not create fb at all\n");
		}

		switch (retry)
		{
		case 0:
			owidth = width;
			oheight = height;
		case 2:
			// Try a different resolution. Hopefully that will work.
			I_ClosestResolution (&width, &height, 8);
			break;

		case 1:
			// Try changing fullscreen mode. Maybe that will work.
			width = owidth;
			height = oheight;
			fullscreen = !fullscreen;
			break;

		default:
			// I give up!
			I_FatalError ("Could not create new screen (%d x %d): %08x", owidth, oheight, hr);
		}

		++retry;
		fb = static_cast<DDrawFB *>(CreateFrameBuffer (width, height, fullscreen, NULL));
	}

	if (fb->IsFullscreen() != fullscreen)
	{
		Video->FullscreenChanged (!fullscreen);
	}

	fb->SetFlash (flashColor, flashAmount);

	return fb;
}

void Win32Video::SetWindowedScale (float scale)
{
	// FIXME
}

// FrameBuffer implementation -----------------------------------------------

DDrawFB::DDrawFB (int width, int height, bool fullscreen)
	: DFrameBuffer (width, height)
{
	if (MemBuffer == NULL)
	{
		return;
	}

	int i;

	LastHR = 0;

	Palette = NULL;
	PrimarySurf = NULL;
	BackSurf = NULL;
	BackSurf2 = NULL;
	Clipper = NULL;
	GammaControl = NULL;
	GDIPalette = NULL;
	ClipRegion = NULL;
	ClipSize = 0;
	BufferCount = 1;

	NeedGammaUpdate = false;
	NeedPalUpdate = false;
	MustBuffer = false;
	BufferingNow = false;
	WasBuffering = false;
	Write8bit = false;
	UpdatePending = false;

	FlashAmount = 0;

	for (i = 0; i < 256; i++)
	{
		PalEntries[i].peRed = GPalette.BaseColors[i].r;
		PalEntries[i].peGreen = GPalette.BaseColors[i].g;
		PalEntries[i].peBlue = GPalette.BaseColors[i].b;
		GammaTable[i] = i;
	}
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);

	MustBuffer = false;

	Windowed = !(static_cast<Win32Video *>(Video)->GoFullscreen (fullscreen));

	if (!CreateResources ())
	{
		if (PrimarySurf != NULL)
		{
			PrimarySurf->Release ();
			PrimarySurf = NULL;
		}
	}
}

DDrawFB::~DDrawFB ()
{
	ReleaseResources ();
}

bool DDrawFB::CreateResources ()
{
	DDSURFACEDESC ddsd = { sizeof(ddsd), };
	HRESULT hr;

	BufferCount = 1;

	if (!Windowed)
	{
		ShowWindow (Window, SW_SHOW);
		hr = DDraw->SetDisplayMode (Width, Height, 8, 0, 0);
		if (FAILED(hr))
		{
			LastHR = hr;
			return false;
		}
		LOG2 ("Mode set to %d x %d\n", Width, Height);

		if (vid_attachedsurfaces)
		{
			if (!CreateSurfacesAttached ())
				return false;
		}
		else
		{
			if (!CreateSurfacesComplex ())
				return false;
		}

/* Can't test the gamma control since my video card doesn't support it. :-(
		DDCAPS caps;

		memset (&caps, 0, sizeof(caps));
		caps.dwSize = sizeof(caps);

		hr = DDraw->GetCaps (&caps, NULL);
		if (SUCCEEDED(hr) && (caps.dwCaps2 & DDCAPS2_PRIMARYGAMMA))
		{
			hr = PrimarySurf->QueryInterface (IID_IDirectDrawGammaControl, (LPVOID *)&GammaControl);
			if (SUCCEEDED(hr))
			{
				LOG ("got gamma control\n");
			}
		}
*/
	}
	else
	{
		MustBuffer = true;

		// Resize the window to match desired dimensions
		SetWindowPos (Window, NULL, 0, 0,
			Width + GetSystemMetrics (SM_CXSIZEFRAME) * 2,
			Height + GetSystemMetrics (SM_CYSIZEFRAME) * 2 +
					 GetSystemMetrics (SM_CYCAPTION)
			, SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);

		// Create the primary surface
		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
		if (FAILED(hr))
		{
			LastHR = hr;
			return false;
		}

		MaybeCreatePalette ();

		// Create the clipper
		hr = DDraw->CreateClipper (0, &Clipper, NULL);
		if (FAILED(hr))
		{
			LastHR = hr;
			return false;
		}
		// Associate the clipper with the window
		Clipper->SetHWnd (0, Window);
		PrimarySurf->SetClipper (Clipper);
		LOG ("Clipper set\n");

		if (!Write8bit)
		{
			// Create the backbuffer
			ddsd.dwFlags        = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
			ddsd.dwWidth        = Width;
			ddsd.dwHeight       = Height;
			ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

			hr = DDraw->CreateSurface (&ddsd, &BackSurf, NULL);
			if (FAILED(hr))
			{
				LastHR = hr;
				return false;
			}
			LockingSurf = BackSurf;
			LOG ("Created backbuf\n");
		}
		else
		{
			LockingSurf = PrimarySurf;
			LOG ("8-bit, so no backbuf\n");
		}
	}
	SetGamma (Gamma);
	SetFlash (Flash, FlashAmount);
	return true;
}

bool DDrawFB::CreateSurfacesAttached ()
{
	DDSURFACEDESC ddsd = { sizeof(ddsd), };
	HRESULT hr;

	LOG ("creating surfaces using AddAttachedSurface\n");

	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;
	hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
	if (FAILED(hr))
	{
		LastHR = hr;
		return false;
	}

	// Under NT 4 and with bad DDraw drivers under 9x (and maybe Win2k?)
	// if the palette is not attached to the primary surface before any
	// back buffers are added to it, colors 0 and 255 will remain black
	// and white respectively.
	MaybeCreatePalette ();

	// Try for triple buffering. Unbuffered output is only allowed if
	// we manage to get triple buffering. Even with double buffering,
	// framerate can slow significantly compared to triple buffering,
	// so we force buffering in that case, which effectively emulates
	// triple buffering (after a fashion).
	if (!AddBackBuf (&BackSurf, 1) || !AddBackBuf (&BackSurf2, 2))
	{
		MustBuffer = true;
	}
	if (BackSurf != NULL)
	{
		DDSCAPS caps = { DDSCAPS_BACKBUFFER, };
		hr = PrimarySurf->GetAttachedSurface (&caps, &LockingSurf);
		if (FAILED (hr))
		{
			LOG1 ("Could not get attached surface: %08x\n", hr);
			if (BackSurf2 != NULL)
			{
				PrimarySurf->DeleteAttachedSurface (0, BackSurf2);
				BackSurf2->Release ();
				BackSurf2 = NULL;
			}
			PrimarySurf->DeleteAttachedSurface (0, BackSurf);
			BackSurf->Release ();
			BackSurf = NULL;
			MustBuffer = true;
			LockingSurf = PrimarySurf;
		}
		else
		{
			BufferCount = (BackSurf2 != NULL) ? 3 : 2;
			LOG ("Got attached surface\n");
		}
	}
	else
	{
		LOG ("No flip chain\n");
		LockingSurf = PrimarySurf;
	}
	return true;
}

bool DDrawFB::AddBackBuf (LPDIRECTDRAWSURFACE *surface, int num)
{
	DDSURFACEDESC ddsd = { sizeof(ddsd), };
	HRESULT hr;

	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth = Width;
	ddsd.dwHeight = Height;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
	hr = DDraw->CreateSurface (&ddsd, surface, NULL);
	if (FAILED(hr))
	{
		LOG2 ("could not create back buf %d: %08x\n", num, hr);
		return false;
	}
	else
	{
		hr = PrimarySurf->AddAttachedSurface (*surface);
		if (FAILED(hr))
		{
			LOG2 ("could not add back buf %d: %08x\n", num, hr);
			(*surface)->Release ();
			*surface = NULL;
			return false;
		}
		else
		{
			LOG1 ("Attachment of back buf %d succeeded\n", num);
		}
	}
	return true;
}

bool DDrawFB::CreateSurfacesComplex ()
{
	DDSURFACEDESC ddsd = { sizeof(ddsd), };
	HRESULT hr;

	LOG ("creating surfaces using a complex primary\n");

	// Try for triple buffering first.
	// If that fails, try for double buffering.
	// If that fails, settle for single buffering.
	// If that fails, then give up.
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY
		| DDSCAPS_FLIP | DDSCAPS_COMPLEX;
	ddsd.dwBackBufferCount = 2;
	hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
	if (FAILED(hr))
	{
		ddsd.dwBackBufferCount = 1;
		hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
		if (FAILED(hr))
		{
			ddsd.dwFlags &= ~DDSD_BACKBUFFERCOUNT;
			ddsd.ddsCaps.dwCaps &= ~DDSCAPS_FLIP | DDSCAPS_COMPLEX;
			ddsd.dwBackBufferCount = 0;
			hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
			if (FAILED (hr))
			{
				LastHR = hr;
				return false;
			}
		}
	}

	if (ddsd.dwBackBufferCount == 0)
	{
		LOG ("No flip chain\n");
		MustBuffer = true;
		LockingSurf = PrimarySurf;
	}
	else
	{
		DDSCAPS caps = { DDSCAPS_BACKBUFFER, };
		hr = PrimarySurf->GetAttachedSurface (&caps, &LockingSurf);
		if (FAILED (hr))
		{
			LOG1 ("Could not get attached surface: %08x\n", hr);
			MustBuffer = true;
			LockingSurf = PrimarySurf;
		}
		else
		{
			LOG ("Got attached surface\n");
			BufferCount = ddsd.dwBackBufferCount + 1;
		}
	}

	MaybeCreatePalette ();
	return true;
}

void DDrawFB::MaybeCreatePalette ()
{
	DDPIXELFORMAT fmt = { sizeof(fmt), };
	HRESULT hr;
	int i;
	
	// If the surface needs a palette, try to create one. If the palette
	// cannot be created, the result is ugly but non-fatal.
	hr = PrimarySurf->GetPixelFormat (&fmt);
	if (SUCCEEDED (hr) && (fmt.dwFlags & DDPF_PALETTEINDEXED8))
	{
		LOG ("Surface is paletted\n");
		GPfx.SetFormat (fmt.dwRGBBitCount,
			fmt.dwRBitMask, fmt.dwGBitMask, fmt.dwBBitMask);

		if (Windowed)
		{
			struct { LOGPALETTE head; PALETTEENTRY filler[255]; } pal;

			LOG ("Writing in a window\n");
			Write8bit = true;
			pal.head.palVersion = 0x300;
			pal.head.palNumEntries = 256;
			memcpy (pal.head.palPalEntry, PalEntries, 256*sizeof(PalEntries[0]));
			for (i = 0; i < 256; i++)
			{
				pal.head.palPalEntry[i].peFlags = 0;
			}
			GDIPalette = CreatePalette (&pal.head);
			LOG ("Created GDI palette\n");
			if (GDIPalette != NULL)
			{
				HDC dc = GetDC (Window);
				SelectPalette (dc, GDIPalette, FALSE);
				RealizePalette (dc);
				ReleaseDC (Window, dc);
				RebuildColorTable ();
			}
		}
		else
		{
			hr = DDraw->CreatePalette (DDPCAPS_8BIT|DDPCAPS_ALLOW256, PalEntries, &Palette, NULL);
			if (FAILED(hr))
			{
				LOG ("Could not create palette\n");
				Palette = NULL;		// NULL it just to be safe
			}
			else
			{
				hr = PrimarySurf->SetPalette (Palette);
				if (FAILED(hr))
				{
					LOG ("Could not attach palette to surface\n");
					Palette->Release ();
					Palette = NULL;
				}
				else
				{
					// The palette was supposed to have been initialized with
					// the correct colors, but some drivers don't do that.
					// (On the other hand, the docs for the SetPalette method
					// don't state that the surface will be set to the
					// palette's colors when it gets set, so this might be
					// legal behavior. Wish I knew...)
					NeedPalUpdate = true;
				}
			}
		}
	}
	else
	{
		LOG ("Surface is direct color\n");
		GPfx.SetFormat (fmt.dwRGBBitCount,
			fmt.dwRBitMask, fmt.dwGBitMask, fmt.dwBBitMask);
		GPfx.SetPalette (GPalette.BaseColors);
	}
}

void DDrawFB::ReleaseResources ()
{
	if (LockCount)
	{
		LockCount = 1;
		Unlock ();
	}

	if (ClipRegion != NULL)
	{
		delete[] ClipRegion;
		ClipRegion = NULL;
	}
	if (Clipper != NULL)
	{
		Clipper->Release ();
		Clipper = NULL;
	}
	if (PrimarySurf != NULL)
	{
		PrimarySurf->Release ();
		PrimarySurf = NULL;
	}
	if (BackSurf != NULL)
	{
		BackSurf->Release ();
		BackSurf = NULL;
	}
	if (BackSurf2 != NULL)
	{
		BackSurf2->Release ();
		BackSurf2 = NULL;
	}
	if (Palette != NULL)
	{
		Palette->Release ();
		Palette = NULL;
	}
	if (GammaControl != NULL)
	{
		GammaControl->Release ();
		GammaControl = NULL;
	}
	if (GDIPalette != NULL)
	{
		HDC dc = GetDC (Window);
		SelectPalette (dc, (HPALETTE)GetStockObject (DEFAULT_PALETTE), TRUE);
		DeleteObject (GDIPalette);
		ReleaseDC (Window, dc);
		GDIPalette = NULL;
	}
}

int DDrawFB::GetPageCount ()
{
	return MustBuffer ? 1 : BufferCount;
}

int DDrawFB::QueryNewPalette ()
{
	LOG ("QueryNewPalette\n");
	if (GDIPalette == NULL)
	{
		if (Write8bit)
		{
			RebuildColorTable ();
		}
		return 0;
	}

	HDC dc = GetDC (Window);
	HPALETTE oldPal = SelectPalette (dc, GDIPalette, FALSE);
	int i = RealizePalette (dc);
	SelectPalette (dc, oldPal, TRUE);
	RealizePalette (dc);
	ReleaseDC (Window, dc);
	if (i != 0)
	{
		RebuildColorTable ();
	}
	return i;
}

void DDrawFB::RebuildColorTable ()
{
	int i;

	if (Write8bit)
	{
		PALETTEENTRY syspal[256];
		HDC dc = GetDC (Window);

		GetSystemPaletteEntries (dc, 0, 256, syspal);

		for (i = 0; i < 256; i++)
		{
			swap (syspal[i].peRed, syspal[i].peBlue);
		}
		for (i = 0; i < 256; i++)
		{
			GPfxPal.Pal8[i] = BestColor ((DWORD *)syspal, PalEntries[i].peRed,
				PalEntries[i].peGreen, PalEntries[i].peBlue);
		}
	}
}

bool DDrawFB::IsValid ()
{
	return PrimarySurf != NULL;
}

bool DDrawFB::Lock ()
{
	return Lock (false);
}

bool DDrawFB::Lock (bool buffered)
{
	bool wasLost;

	LOG2 ("  Lock (%d) <%d>\n", buffered, LockCount);

	if (LockCount++ > 0)
	{
		return false;
	}

	wasLost = false;

	if (MustBuffer || buffered || !AppActive)
	{
		Buffer = MemBuffer;
		Pitch = Width;
		BufferingNow = true;
	}
	else
	{
		LockSurfRes res = LockSurf (NULL);
		
		if (res == NoGood)
		{ // We must have a surface locked before returning,
		  // but we could not lock the hardware surface, so buffer
		  // for this frame.
			Buffer = MemBuffer;
			Pitch = Width;
			BufferingNow = true;
		}
		else
		{
			wasLost = (res == GoodWasLost);
		}
	}

	wasLost = wasLost || (BufferingNow != WasBuffering);
	WasBuffering = BufferingNow;
	return wasLost;
}

bool DDrawFB::Relock ()
{
	return Lock (BufferingNow);
}

void DDrawFB::Unlock ()
{
	LOG1 ("Unlock     <%d>\n", LockCount);

	if (LockCount == 0)
	{
		return;
	}

	if (UpdatePending && LockCount == 1)
	{
		Update ();
	}
	else if (--LockCount == 0)
	{
		if (!BufferingNow)
		{
			LockingSurf->Unlock (NULL);
		}
		Buffer = NULL;
	}
}

DDrawFB::LockSurfRes DDrawFB::LockSurf (LPRECT lockrect)
{
	HRESULT hr;
	DDSURFACEDESC desc = { sizeof(desc), };
	bool wasLost = false;

	LOG ("LockSurf called\n");
	hr = LockingSurf->Lock (lockrect, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
	LOG1 ("result = %08x\n", hr);

	if (hr == DDERR_SURFACELOST)
	{
		wasLost = true;
		hr = PrimarySurf->Restore ();
		if (hr == DDERR_WRONGMODE && Windowed)
		{ // The user changed the screen mode
			ReleaseResources ();
			if (!CreateResources ())
			{
				LOG1 ("Could not recreate framebuffer: %08x", LastHR);
				return NoGood;
			}
		}
		else if (FAILED (hr))
		{
			LOG1 ("Could not restore framebuffer: %08x", hr);
			return NoGood;
		}
		if (BackSurf && FAILED(BackSurf->IsLost ()))
		{
			hr = BackSurf->Restore ();
			if (FAILED (hr))
			{
				I_FatalError ("Could not restore backbuffer: %08x", hr);
			}
		}
		if (BackSurf2 && FAILED(BackSurf2->IsLost ()))
		{
			hr = BackSurf2->Restore ();
			if (FAILED (hr))
			{
				I_FatalError ("Could not restore second backbuffer: %08x", hr);
			}
		}
		hr = LockingSurf->Lock (lockrect, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
		if (hr == DDERR_SURFACELOST && Windowed)
		{ // If this is NT, the user probably opened the Windows NT Security dialog.
		  // If this is not NT, trying to recreate everything from scratch won't hurt.
			ReleaseResources ();
			if (!CreateResources ())
			{
				I_FatalError ("Could not rebuild framebuffer: %08x", LastHR);
			}
			hr = LockingSurf->Lock (lockrect, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
		}
	}
	if (FAILED (hr))
	{ // Still could not restore the surface, so don't draw anything
		//I_FatalError ("Could not lock framebuffer: %08x", hr);
		LOG1 ("Final result: %08x\n", hr);
		return NoGood;
	}
	Buffer = (BYTE *)desc.lpSurface;
	Pitch = desc.lPitch;
	BufferingNow = false;
	return wasLost ? GoodWasLost : Good;
}

void DDrawFB::PartialUpdate (int x, int y, int width, int height)
{
	if (!BufferingNow || MustBuffer || !AppActive)
	{
		return;
	}

	Unlock ();
	Lock ();

	CopyFromBuff (MemBuffer+x+y*Width, Width, width, height, Buffer+x+y*Pitch);
}

void DDrawFB::Update ()
{
	bool pchanged = false;
	int i;

	LOG1 ("Update     <%d>\n", LockCount);

	if (LockCount != 1)
	{
		//I_FatalError ("Framebuffer must have exactly 1 lock to be updated");
		if (LockCount > 0)
		{
			UpdatePending = true;
			--LockCount;
		}
		return;
	}

	DrawRateStuff ();

	if (NeedGammaUpdate)
	{
		NeedGammaUpdate = false;
		if (GammaControl == NULL)
		{
			CalcGamma (Gamma, GammaTable);
			NeedPalUpdate = true;
		}
		else
		{
			// FIXME
		}
	}
	
	if (NeedPalUpdate || vid_palettehack)
	{
		NeedPalUpdate = false;
		if (Palette != NULL || GDIPalette != NULL)
		{
			for (i = 0; i < 256; i++)
			{
				PalEntries[i].peRed = GammaTable[SourcePalette[i].r];
				PalEntries[i].peGreen = GammaTable[SourcePalette[i].g];
				PalEntries[i].peBlue = GammaTable[SourcePalette[i].b];
			}
			if (FlashAmount && GammaControl == NULL)
			{
				DoBlending ((PalEntry *)PalEntries, (PalEntry *)PalEntries,
					256, GammaTable[Flash.b], GammaTable[Flash.g], GammaTable[Flash.r],
					FlashAmount);
			}
			if (Palette != NULL)
			{
				pchanged = true;
			}
			else
			{
				/* Argh! Too slow!
				SetPaletteEntries (GDIPalette, 0, 256, PalEntries);
				HDC dc = GetDC (Window);
				SelectPalette (dc, GDIPalette, FALSE);
				RealizePalette (dc);
				ReleaseDC (Window, dc);
				*/
				RebuildColorTable ();
			}
		}
		else
		{
			for (i = 0; i < 256; i++)
			{
				((PalEntry *)PalEntries)[i].r = GammaTable[SourcePalette[i].r];
				((PalEntry *)PalEntries)[i].g = GammaTable[SourcePalette[i].g];
				((PalEntry *)PalEntries)[i].b = GammaTable[SourcePalette[i].b];
			}
			if (FlashAmount && GammaControl == NULL)
			{
				DoBlending ((PalEntry *)PalEntries, (PalEntry *)PalEntries,
					256, GammaTable[Flash.r], GammaTable[Flash.g], GammaTable[Flash.b],
					FlashAmount);
			}
			GPfx.SetPalette ((PalEntry *)PalEntries);
		}
	}

	if (BufferingNow)
	{
		if (Windowed)
		{
			RECT rect;
			GetClientRect (Window, &rect);
			if (rect.right != 0 && rect.bottom != 0)
			{
				if (Write8bit)
				{
					// Write to primary surface directly
					CopyAndRemap ();
				}
				else
				{
					// Use blit to copy/stretch to window's client rect
					ClientToScreen (Window, (POINT*)&rect.left);
					ClientToScreen (Window, (POINT*)&rect.right);
					if (LockSurf (NULL) != NoGood)
					{
						GPfx.Convert (MemBuffer, Width,
							Buffer, Pitch, Width, Height,
							FRACUNIT, FRACUNIT, 0, 0);
						LockingSurf->Unlock (NULL);
						PrimarySurf->Blt (&rect, BackSurf, NULL, DDBLT_WAIT, NULL);
					}
				}
			}
		}
		else if (AppActive)
		{
			if (LockSurf (NULL) != NoGood)
			{
				CopyFromBuff (MemBuffer, Width, Width, Height, Buffer);
				LockingSurf->Unlock (NULL);
			}
		}
	}
	else
	{
		LockingSurf->Unlock (NULL);
	}
	Buffer = NULL;
	LockCount = 0;
	UpdatePending = false;

	if (!Windowed && !MustBuffer)
	{
		PrimarySurf->Flip (NULL, DDFLIP_WAIT);
	}

	if (pchanged && AppActive)
	{
		Palette->SetEntries (0, 0, 256, PalEntries);
	}
}

void DDrawFB::CopyAndRemap ()
{
	RECT rect;
	DWORD i;
	LPRGNDATA rgn;
	HRESULT hr;
	RECT *clip;

	GetClientRect (Window, &rect);
	ClientToScreen (Window, (POINT*)&rect.left);

	// If the user is dragging a window around on top of us, the clip list
	// size could change between the first call to GetClipList() and the
	// second call, so loop as long as the region is too small.
	do
	{
		hr = Clipper->GetClipList (NULL, NULL, &i);
		if (FAILED(hr))
		{
			I_FatalError ("Could not get clip list size: %08x", hr);
		}
		if (i > ClipSize)
		{
			if (ClipRegion != NULL)
			{
				delete[] ClipRegion;
			}
			ClipRegion = new BYTE[i];
			ClipSize = i;
		}
		rgn = (LPRGNDATA)ClipRegion;
		rgn->rdh.dwSize = sizeof(rgn->rdh);
		i = ClipSize;
		hr = Clipper->GetClipList (NULL, rgn, &i);
	} while (hr == DDERR_REGIONTOOSMALL);
	if (FAILED(hr))
	{
		I_FatalError ("Could not get clip list: %08x", hr);
	}

	if (rgn->rdh.nCount == 0)
	{
		return;
	}

	LOG ("Copy and clip\n");

	// PROBLEM: If a window is being dragged on top of ours, locking the
	// primary surface will not stop that window from dragging (at least
	// under NT 4). Is there any way to stop this? As long as the other
	// window can move while we draw, we might end up drawing on top of it.

	if (LockSurf (&rgn->rdh.rcBound) != NoGood)
	{
		if (rect.right == Width && rect.bottom == Height)
		{
			for (i = rgn->rdh.nCount, clip = (RECT *)rgn->Buffer; i != 0; i--, clip++)
			{
				GPfx.Convert (
					MemBuffer + (clip->left - rect.left) + (clip->top - rect.top) * Width, Width,
					Buffer + (clip->left - rgn->rdh.rcBound.left) + (clip->top - rgn->rdh.rcBound.top) * Pitch, Pitch,
					clip->right - clip->left, clip->bottom - clip->top,
					FRACUNIT, FRACUNIT, 0, 0);
			}
		}
		else
		{
			fixed_t xstep = (Width << FRACBITS) / rect.right;
			fixed_t ystep = (Height << FRACBITS) / rect.bottom;

			for (i = rgn->rdh.nCount, clip = (RECT *)rgn->Buffer; i != 0; i--, clip++)
			{
				fixed_t xfrac = (clip->left - rect.left) * xstep;
				fixed_t yfrac = (clip->top - rect.top) * ystep;

				GPfx.Convert (
					MemBuffer + (xfrac >> FRACBITS) + (yfrac >> FRACBITS) * Width, Width,
					Buffer + (clip->left - rgn->rdh.rcBound.left) + (clip->top - rgn->rdh.rcBound.top) * Pitch, Pitch,
					clip->right - clip->left, clip->bottom - clip->top,
					xstep, ystep, xfrac & (FRACUNIT-1), yfrac & (FRACUNIT-1));
			}
		}

		LockingSurf->Unlock (&rgn->rdh.rcBound);
	}
}

PalEntry *DDrawFB::GetPalette ()
{
	return SourcePalette;
}

void DDrawFB::UpdatePalette ()
{
	NeedPalUpdate = true;
}

bool DDrawFB::SetGamma (float gamma)
{
	LOG1 ("SetGamma %g\n", gamma);
	Gamma = gamma;
	NeedGammaUpdate = true;
	return true;
}

bool DDrawFB::SetFlash (PalEntry rgb, int amount)
{
	Flash = rgb;
	FlashAmount = amount;
	if (GammaControl != NULL)
	{
		// FIXME
	}
	else
	{
		NeedPalUpdate = true;
	}
	return true;
}

void DDrawFB::GetFlash (PalEntry &rgb, int &amount)
{
	rgb = Flash;
	amount = FlashAmount;
}
