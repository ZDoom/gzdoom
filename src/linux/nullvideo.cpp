
// HEADER FILES ------------------------------------------------------------

#include "doomtype.h"

#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"

#include "nullvideo.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class NullFB : public DFrameBuffer
{
public:
	NullFB (int width, int height, bool fullscreen);
	~NullFB ();

	bool Lock (bool buffer);
	bool Relock ();
	void ForceBuffering (bool force);
	void PartialUpdate (int x, int y, int width, int height);
	void Update ();
	PalEntry *GetPalette ();
	void UpdatePalette ();
	bool SetGamma (float gamma);
	bool SetFlash (PalEntry rgb, int amount);
	int GetPageCount ();

	friend class NullVideo;

private:
	PalEntry ThePalette[256];
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

NullVideo::NullVideo (int parm)
{
	lastIt = false;
}

NullVideo::~NullVideo ()
{
}

// This only changes how the iterator lists modes
bool NullVideo::FullscreenChanged (bool fs)
{
	return true;
}

int NullVideo::GetModeCount ()
{
	return 1;
}

void NullVideo::StartModeIterator (int bits)
{
	lastIt = false;
}

bool NullVideo::NextMode (int *width, int *height)
{
	if (!lastIt)
	{
		*width = 320;
		*height = 200;
		lastIt = true;
		return true;
	}
	return false;
}

DFrameBuffer *NullVideo::CreateFrameBuffer (int width, int height, bool fullscreen, DFrameBuffer *old)
{
	if (old != NULL)
	{ // Reuse the old framebuffer if it's attributes are the same
		NullFB *fb = static_cast<NullFB *> (old);
		if (fb->Width == width &&
			fb->Height == height)
		{
			return old;
		}
		delete old;
	}
	NullFB *fb = new NullFB (width, height, fullscreen);
	if (!fb->IsValid ())
	{
		delete fb;
		fb = NULL;
		I_FatalError ("Could not create new screen (%d x %d)", width, height);
	}
	return fb;
}

void NullVideo::SetWindowedScale (float scale)
{
}

// FrameBuffer implementation -----------------------------------------------

NullFB::NullFB (int width, int height, bool fullscreen)
	: DFrameBuffer (width, height)
{
}

NullFB::~NullFB ()
{
}

int NullFB::GetPageCount ()
{
	return 1;
}

bool NullFB::Lock (bool buffered)
{
	return DSimpleCanvas::Lock ();
}

bool NullFB::Relock ()
{
	return DSimpleCanvas::Lock ();
}

void NullFB::PartialUpdate (int x, int y, int width, int height)
{
}

void NullFB::Update ()
{
	Unlock ();
}

PalEntry *NullFB::GetPalette ()
{
	return ThePalette;
}

void NullFB::UpdatePalette ()
{
}

bool NullFB::SetGamma (float gamma)
{
	return true;
}

bool NullFB::SetFlash (PalEntry rgb, int amount)
{
	return true;
}
