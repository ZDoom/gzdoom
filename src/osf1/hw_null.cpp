#define HARDWARELIB
#include "hardware.h"

#ifdef OSF1
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <unistd.h>
#ifdef OSF1
#undef _XOPEN_SOURCE_EXTENDED
#endif

// KEYBOARD -----------------------------------------------------------------

class NullKeyboard : public IKeyboard
{
public:
    NullKeyboard () {}
    ~NullKeyboard () {}
    void ProcessInput (bool consoleOpen) { usleep (1000); }
	void SetKeypadRemapping (bool remap) {}
};

// VIDEO ----------------------------------------------------------------------

class NullVideo : public IVideo
{
public:
	NullVideo ();
	~NullVideo ();

	EDisplayType GetDisplayType () { return DISPLAY_FullscreenOnly; }
	bool FullscreenChanged (bool fs) { return false; }
	void SetWindowedScale (float scale) {}
	bool CanBlit () { return false; }

	void SetPalette (DWORD *palette) {}
	void UpdateScreen (DCanvas *canvas) {}
	void ReadScreen (byte *block) {}

	bool CheckResolution (int width, int height, int bits);
	int GetModeCount () { return 1; }
	void StartModeIterator (int bits);
	bool NextMode (int *width, int *height);
	bool SetMode (int width, int height, int bits, bool fs);

	bool AllocateSurface (DCanvas *scrn, int width, int height, int bits, bool primary);
	void ReleaseSurface (DCanvas *scrn);
	void LockSurface (DCanvas *scrn);
	void UnlockSurface (DCanvas *scrn);
	bool Blit (DCanvas *src, int sx, int sy, int sw, int sh,
			   DCanvas *dst, int dx, int dy, int dw, int dh);

private:
	static int SortModes (const void *a, const void *b);

	void MakeModesList ();

	int m_IteratorMode;
	int m_IteratorBits;

	struct Chain
	{
		DCanvas *canvas;
		Chain *next;
	} *m_Chain;

	struct ModeInfo
	{
		int width, height, bits;
	} m_Modes[1];
};

NullVideo::NullVideo ()
{
	m_IteratorMode = m_IteratorBits = 0;
	MakeModesList ();
}

NullVideo::~NullVideo ()
{
	while (m_Chain)
		ReleaseSurface (m_Chain->canvas);
}

bool NullVideo::SetMode (int width, int height, int bits, bool fs)
{
    int i;

	if (width != 320 || height != 200 || bits != 8)
		return false;

	return true;
}

void NullVideo::MakeModesList ()
{
	m_Modes[0].width = 320;
	m_Modes[0].height = 200;
	m_Modes[0].bits = 8;
}

void NullVideo::StartModeIterator (int bits)
{
	m_IteratorMode = 0;
	m_IteratorBits = bits;
}

bool NullVideo::NextMode (int *width, int *height)
{
    if (m_IteratorMode < 1)
    {
		while (m_IteratorMode < 1 &&
			   m_Modes[m_IteratorMode].bits != m_IteratorBits)
		{
			m_IteratorMode++;
		}

		if (m_IteratorMode < 1)
		{
			*width = m_Modes[m_IteratorMode].width;
			*height = m_Modes[m_IteratorMode].height;
			m_IteratorMode++;
			return true;
		}
    }
    return false;
}

bool NullVideo::AllocateSurface (DCanvas *scrn, int width, int height,
								 int bits, bool primary)
{
    if (scrn->m_Private)
		ReleaseSurface (scrn);

    scrn->width = width;
    scrn->height = height;
    scrn->is8bit = (bits == 8) ? true : false;
    scrn->bits = 8;
    scrn->m_LockCount = 0;
    scrn->m_Palette = NULL;

    if (!scrn->is8bit)
		return false;

	scrn->m_Private = new byte[width*height*(bits>>3)];

    Chain *tracer = new Chain;
    tracer->canvas = scrn;
    tracer->next = m_Chain;
    m_Chain = tracer;

    return true;
}

void NullVideo::ReleaseSurface (DCanvas *scrn)
{
    struct Chain *thisone, **prev;

    if (scrn->m_Private)
    {
		delete[] (byte *)scrn->m_Private;
		scrn->m_Private = NULL;
    }

    scrn->DetachPalette ();

    thisone = m_Chain;
    prev = &m_Chain;
    while (thisone && thisone->canvas != scrn)
    {
		prev = &thisone->next;
		thisone = thisone->next;
	}
	if (thisone)
	{
		*prev = thisone->next;
		delete thisone;
	}
}

void NullVideo::LockSurface (DCanvas *scrn)
{
    if (scrn->m_Private)
    {
		scrn->buffer = (byte *)scrn->m_Private;
		scrn->pitch = scrn->width;
    }
    else
    {
		scrn->m_LockCount--;
    }
}

void NullVideo::UnlockSurface (DCanvas *scrn)
{
	if (scrn->m_Private)
	{
		scrn->buffer = NULL;
	}
}

bool NullVideo::Blit (DCanvas *src, int sx, int sy, int sw, int sh,
					  DCanvas *dst, int dx, int dy, int dw, int dh)
{
	return false;
}

// ----------------------------------------------------------------------------

static void *QueryInterface (EInterfaceType type, int parm)
{
	void *res;

	switch (type)
	{
	case INTERFACE_Video:		res = new NullVideo;			break;
	case INTERFACE_Keyboard:	res = new NullKeyboard;			break;
	default:					return NULL;
	}
	if (res == NULL)
		return NULL;
	switch (type)
	{
	case INTERFACE_Video:		delete (NullVideo *)res;		break;
	case INTERFACE_Keyboard:	delete (NullKeyboard *)res;		break;
	}
	return NULL;
}

extern "C" queryinterface_t PrepNullLibrary (IEngineGlobals *globs)
{
	ge = globs;
	return QueryInterface;
}
