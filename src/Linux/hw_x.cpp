#define HARDWARELIB
#include "hardware.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef OSF1
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <unistd.h>
#ifdef OSF1
#undef _XOPEN_SOURCE_EXTENDED
#endif

#include <sys/ipc.h>
#include <sys/shm.h>
#include <dlfcn.h>

#include <Hermes/Hermes.h>

extern "C" {
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef USE_SHM
#include <X11/extensions/XShm.h>
int XShmGetEventBase (Display *dpy);
#endif
#ifdef USE_DGA
#include <X11/extensions/xf86dga.h>
#ifdef USE_VIDMODE
#define __EMX__	// Avoid problems re-typedefing BOOL
#include <X11/extensions/xf86vmode.h>
#endif
#endif
}

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "c_cvars.h"
#include "c_console.h"
#include "d_main.h"

#include "i_system.h"

#ifdef USEASM
#include "expandorama.h"
#endif

#define MIN(a,b)	((a)<(b)?(a):(b))

extern const char *KeyNames[NUM_KEYS];
extern void STACK_ARGS call_terms ();

static bool CanUnload = true;
static int globalres;
#ifdef USE_SHM
static bool ShmFinished;
#endif

class XMouse : public IMouse
{
	friend class XKeyboard;
	friend class XVideo;

public:
	XMouse ();
	~XMouse ();
	void ProcessInput (bool active);
	void SetGrabbed (bool grabbed);

private:
	bool m_ShouldGrab;
	bool m_IsGrabbed;
	bool m_JustGrabbed;
	int m_LastX;
	int m_LastY;
	unsigned int m_LastButtons;

	void DoGrab (bool grabbed);
};

#define DIK_NUMPAD7		0x47
#define DIK_NUMPAD8		0x48
#define DIK_NUMPAD9		0x49
#define DIK_SUBTRACT	0x4a
#define DIK_NUMPAD4		0x4b
#define DIK_NUMPAD5		0x4c
#define DIK_NUMPAD6		0x4d
#define DIK_ADD			0x4e
#define DIK_NUMPAD1		0x4f
#define DIK_NUMPAD2		0x50
#define DIK_NUMPAD3		0x51
#define DIK_NUMPAD0		0x52
#define DIK_DECIMAL		0x53

#define DIK_1			2
#define DIK_2			3
#define DIK_3			4
#define DIK_4			5
#define DIK_5			6
#define DIK_6			7
#define DIK_7			8
#define DIK_8			9
#define DIK_9			10
#define DIK_0			11
#define DIK_PERIOD		0x34

class XKeyboard : public IKeyboard
{
	friend class XMouse;
	friend class XVideo;

public:
    XKeyboard ();
    ~XKeyboard ();
    void ProcessInput (bool consoleOpen);
	void SetKeypadRemapping (bool remap);

private:
	bool m_RemapKeypad;
    bool m_Inited;
	bool m_Focused;

	byte m_ToDICodes[256];
	byte m_ToANSI[256];
	byte m_KeyState[256/8];
	bool m_GotKeymap;

	void LoadKeymap ();
	void FillKeycode (event_t &ev, KeyCode keycode);
	static int CompatXLateKey (Display *disp, KeyCode code);
	static void PrepCompatXLateKey ();
	static byte KPRemap[2][2][DIK_DECIMAL-DIK_NUMPAD7+1];
	static struct CompatMap {
		KeySym Sym;
		union {
			const char *Label;
			int DICode;
		} U;
	} m_CompatXLate[];
	static int Comparator (const void *a, const void *b);
	static int m_CompatXLateCount;
};

static XMouse *ActiveMouse;
static XKeyboard *ActiveKeys;

// VIDEO ----------------------------------------------------------------------

// class AnImage

class AnImage
{
public:
	AnImage (int width, int height) { m_Width = width; m_Height = height; }
	virtual ~AnImage() {}
	virtual bool IsValid() = 0;
	virtual void Put (Window w, GC gc) = 0;
	virtual byte *DataPtr() = 0;
	virtual int Pitch() = 0;
#ifdef USE_SHM
	virtual bool NeedShmWait() = 0;
#endif

	inline int Width () { return m_Width; }
	inline int Height () { return m_Height; }
protected:
	int m_Width, m_Height;
};

// class ShmImage

#ifdef USE_SHM
class ShmImage : public AnImage
{
public:
	ShmImage (Display *disp, int screen, int width, int height);
	~ShmImage ();
	bool IsValid ();
	void Put (Window, GC);
	byte *DataPtr () { return (byte *)m_Image->data; }
	int Pitch () { return m_Image->bytes_per_line; }
	bool NeedShmWait () { return true; }

private:
	static int TempErrorHandler (Display *, XErrorEvent *);
	static int (*m_OldHandler) (Display *, XErrorEvent *);
	static bool m_AttachFail;

	XImage *m_Image;
	XShmSegmentInfo m_ShmInfo;
	bool m_Good;
	Display *m_Display;
};

bool ShmImage::m_AttachFail;
int (*ShmImage::m_OldHandler) (Display *, XErrorEvent *);

ShmImage::ShmImage (Display *disp, int screen, int width, int height)
	: AnImage (width, height)
{
	m_Good = false;
	m_Display = disp;
	m_Image = XShmCreateImage (disp, DefaultVisual (disp, screen),
							   DefaultDepth (disp, screen), ZPixmap,
							   NULL, &m_ShmInfo, width, height);
	if (m_Image == NULL)
		return;

	m_ShmInfo.shmid = shmget (IPC_PRIVATE,
							  m_Image->bytes_per_line * m_Image->height,
							  IPC_CREAT | 0777);
	if (m_ShmInfo.shmid < 0)
		return;

	m_ShmInfo.shmaddr = m_Image->data = (char *)shmat (m_ShmInfo.shmid, 0, 0);
	if (m_ShmInfo.shmaddr == NULL)
		return;

	m_ShmInfo.readOnly = True;

	m_AttachFail = false;
	m_OldHandler = XSetErrorHandler (TempErrorHandler);
	if (XShmAttach (disp, &m_ShmInfo))
	{
		XSync (m_Display, false);
		if (!m_AttachFail)
			m_Good = true;
	}
	XSetErrorHandler (m_OldHandler);
}

ShmImage::~ShmImage ()
{
	if (m_Good)
		XShmDetach (m_Display, &m_ShmInfo);
	if (m_Image)
		XDestroyImage (m_Image);
	if (m_ShmInfo.shmaddr)
		shmdt (m_ShmInfo.shmaddr);
	if (m_ShmInfo.shmid >= 0)
		shmctl (m_ShmInfo.shmid, IPC_RMID, 0);
}

bool ShmImage::IsValid ()
{
	return m_Good;
}

void ShmImage::Put (Window w, GC gc)
{
	XShmPutImage (m_Display, w, gc, m_Image, 0, 0, 0, 0,
				  m_Image->width, m_Image->height, True);
	ShmFinished = false;
}

int ShmImage::TempErrorHandler (Display *disp, XErrorEvent *err)
{
	if (err->error_code == BadAccess)
	{
		m_AttachFail = true;
		return 0;
	}
	else
		m_OldHandler (disp, err);
	return 1;
}
#endif

// class PlainImage

class PlainImage : public AnImage
{
public:
	PlainImage (Display *disp, int screen, int width, int height);
	~PlainImage ();
	bool IsValid ();
	void Put (Window, GC);
	byte *DataPtr () { return (byte *)m_Image->data; }
	int Pitch () { return m_Image->bytes_per_line; }
#ifdef USE_SHM
	bool NeedShmWait () { return false; }
#endif

private:
	XImage *m_Image;
	Display *m_Display;
};

PlainImage::PlainImage (Display *disp, int screen, int width, int height)
	: AnImage (width, height)
{
	Visual *vis = DefaultVisual (disp, screen);
	int bits = DefaultDepth (disp, screen);
	m_Display = disp;
	m_Image = NULL;
	m_Image = XCreateImage (disp, vis, bits, ZPixmap, 0, NULL,
							width, height, bits != 24 ? bits : 32, 0);
	if (m_Image)
		m_Image->data = new char[m_Image->bytes_per_line * height];
}

PlainImage::~PlainImage ()
{
	if (m_Image)
	{
		if (m_Image->data)
			delete[] m_Image->data;
		XDestroyImage (m_Image);
	}
}

bool PlainImage::IsValid ()
{
	return m_Image && m_Image->data;
}

void PlainImage::Put (Window w, GC gc)
{
	XPutImage (m_Display, w, gc, m_Image, 0, 0, 0, 0,
			   m_Image->width, m_Image->height);
	XSync (m_Display, False);
}

// class XVideo

class XVideo : public IVideo
{
	friend class XKeyboard;
	friend class XMouse;

public:
	XVideo ();
	~XVideo ();

	EDisplayType GetDisplayType ();
	bool FullscreenChanged (bool fs);
	bool CanBlit () { return true; }
	void SetWindowedScale (float scale) { m_WinScale = scale; }

	void SetPalette (DWORD *palette);
	void UpdateScreen (DCanvas *canvas);
	void ReadScreen (byte *block);

	bool CheckResolution (int width, int height, int bits);
	int GetModeCount ();
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
	bool MakeModesList ();
	static int SortModes (const void *a, const void *b);
	void FreeModes ();
	void GetFormat (HermesFormat *format, Display *disp, int screen) const;
	void CreateWindow (int width, int height);
	Cursor CreateNullCursor ();
	void ShowCursor ();
	void HideCursor ();
	void SetWindowed (int mode);

	int m_NumModes;
	int m_IteratorMode;
	int m_IteratorBits;

	int m_DisplayWidth;
	int m_DisplayHeight;
	int m_DisplayBits;
#ifdef USE_VIDMODE
	byte *m_DisplayBase;
	int m_DisplayPitch;
	int m_DisplayRows;
	int m_BankSize;
	int m_MemSize;
#endif
	bool m_IteratorFS;
#ifdef USEASM
	bool m_FastScale;
#endif

	int m_NeedPalChange;
	XColor m_Colors[256];

	Display *m_Display;
	int m_Screen;
	Window m_Window;
	Window m_Root;
	Colormap m_Cmap;
	GC m_GC;
	Atom m_DeleteAtom;
	Cursor m_Cursor;
	float m_WinScale;

	HermesHandle m_HermesPalette;
	HermesHandle m_HermesConverter;
	HermesFormat m_SourceFormat;
	HermesFormat m_DestFormat;
#ifdef USE_SHM	
	bool m_DoShm;
	int m_ShmEventType;
#endif

#ifdef USE_DGA
	bool m_HaveDGA;
#ifdef USE_VIDMODE
	bool m_HaveVidMode;
	bool m_Fullscreen;
	bool m_Scale;
#endif
#endif

	AnImage *m_Image;
	DCanvas *m_LastUpdatedCanvas;

	struct Chain
	{
		DCanvas *canvas;
		Chain *next;
	} *m_Chain;

	struct ModeInfo
	{
		int width, height;
		bool scaled;
		int modeline;
	} *m_Modes;

	static struct MiniModeInfo
	{
		int width, height;
	} m_WinModes[];

#ifdef USE_VIDMODE
	XF86VidModeModeInfo **m_VidModes;
	int m_NumVidModes;
	int m_UserMode;
	void RestoreVidMode ();
	bool SetFullscreen (int mode);
#endif
};

static XVideo *ActiveVideo;

XVideo::MiniModeInfo XVideo::m_WinModes[] =
{
	{ 320, 200 },
	{ 320, 240 },
	{ 400, 300 },
	{ 480, 360 },
	{ 512, 384 },
	{ 640, 400 },
	{ 640, 480 },
	{ 800, 600 },
	{ 960, 720 },
	{ 1024, 768 },
	{ 1152, 864 },
	{ 1280, 1024 },
	{ 0, 0 }
};

XVideo::XVideo ()
{
	globalres = 1;	// Assume failure

	m_IteratorMode = m_IteratorBits = 0;
	m_WinScale = 0.f;
	m_IteratorFS = false;
	m_DisplayWidth = m_DisplayHeight = m_DisplayBits = 0;
	m_LastUpdatedCanvas = NULL;
	m_NeedPalChange = false;
	m_Display = NULL;
	m_Screen = 0;
	m_Window = 0;
	m_Root = 0;
	m_Cmap = 0;
	m_GC = NULL;
#ifdef USE_SHM
	m_DoShm = false;
	m_ShmEventType = 0;
#endif
#ifdef USE_DGA
	m_HaveDGA = false;
#ifdef USE_VIDMODE
	m_HaveVidMode = false;
	m_VidModes = NULL;
	m_Fullscreen = false;
	m_NumVidModes = 0;
	m_Scale = false;
#endif
#endif
	m_Image = NULL;
	m_Chain = NULL;
	m_Modes = NULL;
	m_Cursor = None;
	m_HermesPalette = 0;
#ifdef USEASM
	m_FastScale = false;
#endif

	if (!Hermes_Init ())
		ge->FatalError ("hw_x: Failed to initialize Hermes");
	if (!(m_HermesPalette = Hermes_PaletteInstance ()))
		ge->FatalError ("hw_x: Could not get Hermes palette");
	if (!(m_HermesConverter = Hermes_ConverterInstance (0)))
		ge->FatalError ("hw_x: Could not get Hermes converter");

	// open the display
	m_Display = XOpenDisplay (NULL);
	if (m_Display == NULL)
		return;

	m_Screen = DefaultScreen (m_Display);

	m_Root = XRootWindow (m_Display, m_Screen);

	m_DisplayWidth = DisplayWidth (m_Display, m_Screen);
	m_DisplayHeight = DisplayHeight (m_Display, m_Screen);
	m_DisplayBits = DisplayPlanes (m_Display, m_Screen);
	GetFormat (&m_DestFormat, m_Display, m_Screen);
#ifdef USEASM
	ExpandSetMode (m_DestFormat.bits,
				   m_DestFormat.r, m_DestFormat.g, m_DestFormat.b);
#endif
	m_SourceFormat.r = m_SourceFormat.g = m_SourceFormat.b =
		m_SourceFormat.a = 0;
	m_SourceFormat.bits = 8;
	m_SourceFormat.indexed = 1;

	m_DeleteAtom = XInternAtom (m_Display, "WM_DELETE_WINDOW", False);

#ifdef USE_SHM
	// check for the MITSHM extension
#ifdef NO_XSHMQUERYEXTENSION
	{
		int major, minor;
		Bool pixmaps;
		XShmQueryVersion (m_Display, &major, &minor, &pixmaps);
		m_DoShm = !!pixmaps;
		printf ("Server has Shm version %d.%d\n", major, minor);
	}
#else
	m_DoShm = XShmQueryExtension (m_Display) ? true : false;
#endif
	// even if it's available, make sure it's a local connection
	if (m_DoShm)
	{
		char *displayname = getenv ("DISPLAY");
		if (displayname)
		{
			char dispcopy[128];
			char *d;
			strcpy (dispcopy, displayname);
			d = strchr (dispcopy, ':');
			if (d)
				*d = 0;
			if (!(!stricmp (dispcopy, "unix") || !*dispcopy))
				m_DoShm = false;
		}
	}

	if (m_DoShm)
		ge->Printf (PRINT_HIGH, "hw_x: will use MIT-SHM extension\n");
#endif

#ifdef USE_DGA
	// Check for the DGA extension
	int dummy1, dummy2;

	m_HaveDGA = XF86DGAQueryExtension (m_Display, &dummy1, &dummy2)
		? true : false;

	if (m_HaveDGA)
	{
		ge->Printf (PRINT_HIGH, "hw_x: will use DGA extension\n");

#ifdef USE_VIDMODE
		// Check for the VidMode extension
		m_HaveVidMode = XF86VidModeQueryExtension (m_Display, &dummy1, &dummy2)
			? true : false;

		if (m_HaveVidMode)
			ge->Printf (PRINT_HIGH, "hw_x: VidMode extension is available\n");
#endif
	}
#endif

	// create the colormap
	Visual *vis = DefaultVisual (m_Display, m_Screen);

	if (vis->c_class == PseudoColor && m_DestFormat.indexed)
	{
		m_Cmap = XCreateColormap (m_Display, RootWindow (m_Display, m_Screen),
								  vis, AllocAll);
		if (!m_Cmap)
		{
			ge->Printf (PRINT_HIGH, "hw_x: could not create colormap");
			return;
		}

		int i;
		for (i = 0; i < 256; i++)
		{
			m_Colors[i].pixel = i;
			m_Colors[i].flags = DoRed | DoGreen | DoBlue;
			m_Colors[i].red =
				m_Colors[i].green =
				m_Colors[i].blue = i & 1 ? 65535 : 0;
		}
		XStoreColors (m_Display, m_Cmap, m_Colors, 256);
	}	
	else
		m_Cmap = 0;

	if (!MakeModesList ())
		return;

	seteuid (getuid ());
	globalres = 0;
}

XVideo::~XVideo ()
{
	ActiveVideo = NULL;
	if (m_HermesConverter)
		Hermes_ConverterReturn (m_HermesConverter);
	if (m_HermesPalette)
		Hermes_PaletteReturn (m_HermesPalette);
	Hermes_Done ();
	if (m_Display)
	{
#ifdef USE_VIDMODE
		RestoreVidMode ();
#endif
		FreeModes ();
		if (m_Cursor != None)
		{
			XFreeCursor (m_Display, m_Cursor);
		}
		if (m_Image)
			delete m_Image;
		while (m_Chain)
			ReleaseSurface (m_Chain->canvas);
		if (m_Window)
		{
			XUnmapWindow (m_Display, m_Window);
			XSync (m_Display, False);
			XDestroyWindow (m_Display, m_Window);
		}
		if (m_Cmap)
		{
			XFreeColormap (m_Display, m_Cmap);
		}
		XCloseDisplay (m_Display);
	}
}

Cursor XVideo::CreateNullCursor ()
{
	Pixmap cursorMask;
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;

	cursorMask = XCreatePixmap (m_Display, m_Window, 1, 1, 1);
	xgc.function = GXclear;
	gc = XCreateGC (m_Display, cursorMask, GCFunction, &xgc);
	XFillRectangle (m_Display, cursorMask, gc, 0, 0, 1, 1);
	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 4;
	cursor = XCreatePixmapCursor (m_Display, cursorMask, cursorMask,
								  &dummycolour, &dummycolour, 0, 0);
	XFreePixmap (m_Display, cursorMask);
	XFreeGC (m_Display, gc);
	return cursor;
}

void XVideo::GetFormat (HermesFormat *format, Display *d, int screen) const
{
	Visual *vis = DefaultVisual (d, screen);

	if (vis)
	{
		format->r = vis->red_mask;
		format->g = vis->green_mask;
		format->b = vis->blue_mask;
		format->a = 0;
		format->indexed = vis->c_class == DirectColor || vis->c_class == TrueColor ? 0 : 1;
		format->bits = DisplayPlanes (d, screen);
		if (format->bits == 24)
			format->bits = 32;
	}
	else
	{
		format->r = format->g = format->b = format->a = 0;
		format->bits = 8;
		format->indexed = 1;
	}
}

EDisplayType XVideo::GetDisplayType ()
{
#ifdef USE_VIDMODE
	if (m_HaveVidMode && m_NumModes)
		return DISPLAY_Both;
	else
#endif
		return DISPLAY_WindowOnly;
}

bool XVideo::SetMode (int width, int height, int bits, bool fullscreen)
{
    int i;

	m_NeedPalChange = 1;
#ifdef USE_VIDMODE
	if (m_NumModes && fullscreen)
	{
		for (i = 0; i < m_NumModes; i++)
		{
			if (m_Modes[i].width == width && m_Modes[i].height == height)
				break;
		}
		if (i == m_NumModes || !SetFullscreen (i))
			return false;
	}
	else
#endif
	{
#ifdef USE_VIDMODE
		RestoreVidMode ();
#endif
		for (i = 0; m_WinModes[i].width; i++)
		{
			if (m_WinModes[i].width == width &&
				m_WinModes[i].width < m_DisplayWidth &&
				m_WinModes[i].height == height &&
				m_WinModes[i].height < m_DisplayHeight)
			{
				break;
			}
		}

		if (m_WinModes[i].width == 0)
			return false;

		SetWindowed (i);
	}
#ifdef USE_SHM
	ShmFinished = true;
#endif

	return true;
}

void XVideo::SetWindowed (int i)
{
	int width = m_WinModes[i].width;
	int height = m_WinModes[i].height;

	width = (int)((float)width * m_WinScale);
	height = (int)((float)height * m_WinScale);

	if (!m_Window)
		CreateWindow (width, height);
	else
   	{
		XWindowAttributes attribs;

		XGetWindowAttributes (m_Display, m_Window, &attribs);
		XSync (m_Display, False);

		if (attribs.x + width > m_DisplayWidth)
			attribs.x = m_DisplayWidth - width;
		if (attribs.y + height > m_DisplayHeight)
			attribs.y = m_DisplayHeight - height;
	
		XResizeWindow (m_Display, m_Window, width, height);
	}

	XSizeHints hint;
	hint.flags = PSize | PMinSize | PMaxSize;
	hint.min_width = hint.max_width = hint.base_width = width;
	hint.min_height = hint.max_height = hint.base_height = height;
	XSetWMNormalHints (m_Display, m_Window, &hint);

	if (m_Image)
		delete m_Image;

#ifdef USE_SHM
	switch (m_DoShm)
	{
	case true:
		m_ShmEventType = XShmGetEventBase (m_Display) + ShmCompletion;
		m_Image = new ShmImage (m_Display, m_Screen, width, height);
		if (m_Image->IsValid ())
			break;
		ge->Printf (PRINT_HIGH, "hw_x: falling back to normal XImage\n");
		delete m_Image;
	case false:
#else
	{
#endif
		m_Image = new PlainImage (m_Display, m_Screen, width, height);
		if (!m_Image->IsValid ())
		{
			ge->FatalError ("hw_x: failed to create image");
			return;
		}
	}

#ifdef USEASM
	m_FastScale = (m_WinScale == 2.f);
#endif
}

#ifdef USE_VIDMODE
void XVideo::RestoreVidMode ()
{
	if (m_Fullscreen)
	{
		XUngrabPointer (m_Display, CurrentTime);
		XUngrabKeyboard (m_Display, CurrentTime);
		XF86DGADirectVideo (m_Display, m_Screen, 0);
		XF86VidModeSwitchToMode (m_Display, m_Screen, m_VidModes[m_UserMode]);
		XFlush (m_Display);
		ShowCursor ();
		m_Fullscreen = false;
		if (ActiveMouse)
			ActiveMouse->m_IsGrabbed = false;
		m_DisplayWidth = DisplayWidth (m_Display, m_Screen);
		m_DisplayHeight = DisplayHeight (m_Display, m_Screen);
	}
}

bool XVideo::SetFullscreen (int mode)
{
	bool res;
	int dotclock, i;
	XF86VidModeModeLine curmode;

	if (!m_Fullscreen)
	{
		XF86VidModeGetModeLine (m_Display, m_Screen, &dotclock, &curmode);
		for (i = 0; i < m_NumVidModes; i++)
		{
			if (m_VidModes[i]->hdisplay == curmode.hdisplay &&
				m_VidModes[i]->vdisplay == curmode.vdisplay)
			{
				m_UserMode = i;
				break;
			}
		}
		if (i == m_NumVidModes)
			return false;
	}

	res = XF86VidModeSwitchToMode (m_Display, m_Screen,
								   m_VidModes[m_Modes[mode].modeline])
		? true : false;

	XFlush (m_Display);

	if (res)
	{
		if (!m_Window)
			CreateWindow (32, 32);

		if (m_Image)
			delete m_Image, m_Image = NULL;

		m_Fullscreen = true;
		m_DisplayWidth = m_VidModes[m_Modes[mode].modeline]->hdisplay;
		m_DisplayHeight = m_VidModes[m_Modes[mode].modeline]->vdisplay;
		m_Scale = m_Modes[mode].scaled;

		seteuid (0);
		XF86DGADirectVideo (m_Display, m_Screen,
			XF86DGADirectMouse | XF86DGADirectKeyb);
		XF86DGAGetVideo (m_Display, m_Screen,
						 (char **)&m_DisplayBase, &m_DisplayPitch, &m_BankSize,
						 &m_MemSize);
		m_DisplayPitch *= ((m_DisplayBits==24)?32:m_DisplayBits) / 8;
		m_DisplayPitch = (m_DisplayPitch + 3) & ~3;
		m_DisplayRows = m_BankSize / m_DisplayPitch;

		// Work-around for XFree86 installing its own atexit handler that
		// doesn't let subsequent atexit handlers run.
		atexit (call_terms);
		CanUnload = false;

		XF86DGASetViewPort (m_Display, m_Screen, 0, 0);
		XF86DGADirectVideo (m_Display, m_Screen,
			XF86DGADirectGraphics | XF86DGADirectMouse | XF86DGADirectKeyb);
		XF86DGASetVidPage (m_Display, m_Screen, 0);
		seteuid (getuid ());

		XGrabKeyboard (m_Display, m_Window, True, GrabModeAsync,
					   GrabModeAsync, CurrentTime);
		XGrabPointer (m_Display, m_Window, True,
					  ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
					  GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
		HideCursor ();
		if (m_Cmap)
			XInstallColormap (m_Display, m_Cmap);
	}

	return res;
}
#endif

void XVideo::CreateWindow (int width, int height)
{
	Visual *vis = DefaultVisual (m_Display, m_Screen);
	XSetWindowAttributes attribs;
	unsigned long attribmask;

	if ( (attribs.colormap = m_Cmap) )
		attribmask = CWColormap;
	else
		attribmask = 0;

	attribmask |= CWEventMask;
	attribs.event_mask = StructureNotifyMask;

	// create the window
	m_Window = XCreateWindow (m_Display,
							  RootWindow (m_Display, m_Screen),
							  0, 0,
							  width, height,
							  0,
							  DefaultDepth (m_Display, m_Screen),
							  InputOutput,
							  vis,
							  attribmask,
							  &attribs);
							  
	if (m_Window == 0)
	{
		ge->FatalError ("hw_x: failed to create window");
		return;
	}

	XTextProperty name;
	char *str = "ZDOOM (" __DATE__ ")";
	XStringListToTextProperty (&str, 1, &name);
	XSetWMName (m_Display, m_Window, &name);
	XFlush (m_Display);
	XFree (name.value);

	XSetWMProtocols (m_Display, m_Window, &m_DeleteAtom, 1);

	m_Cursor = CreateNullCursor ();

	// create the GC
	int valuemask = GCGraphicsExposures;
	XGCValues xgcvalues;
	xgcvalues.graphics_exposures = False;
	m_GC = XCreateGC (m_Display, m_Window, valuemask, &xgcvalues);
	if (m_GC == NULL)
	{
		ge->FatalError ("hw_x: failed to create GC");
		return;
	}

	// map the window
	XMapWindow (m_Display, m_Window);

	// wait until it is OK to draw
	bool oktodraw = false;
	while (!oktodraw)
	{
		XEvent X_event;
		XNextEvent (m_Display, &X_event);
		if (X_event.type == MapNotify)
			oktodraw = true;
	}

	XSelectInput (m_Display, m_Window,
				  KeyPressMask | KeyReleaseMask | FocusChangeMask
#ifdef USE_DGA
				  | (m_HaveDGA ? PointerMotionMask | ButtonPressMask
					 | ButtonReleaseMask : 0)
#endif
		);

	if (ActiveKeys)
	{
		Window focus;
		int revert = 0;

		XGetInputFocus (m_Display, &focus, &revert);
		ActiveKeys->m_Focused = m_Window == focus;
	}
}

void XVideo::ShowCursor ()
{
	XUndefineCursor (m_Display, m_Window);
}

void XVideo::HideCursor ()
{
	XDefineCursor (m_Display, m_Window, m_Cursor);
}

void XVideo::SetPalette (DWORD *pal)
{
    if (!pal)
		return;

	int32 *palentries = Hermes_PaletteGet (m_HermesPalette);
	memcpy (palentries, pal, 256*4);
	Hermes_PaletteInvalidateCache (m_HermesPalette);
	m_NeedPalChange++;
}

void XVideo::UpdateScreen (DCanvas *canvas)
{
	m_LastUpdatedCanvas = canvas;
#ifdef USEASM
	if (!m_FastScale
#ifdef USE_VIDMODE
		&& (!m_Fullscreen || !m_Scale)
#endif
	   )
#endif
	{
		if (0 == Hermes_ConverterRequest (m_HermesConverter,
										  &m_SourceFormat, &m_DestFormat))
		{
			ge->FatalError ("Unsupported Hermes converter formats");
		}
		Hermes_ConverterPalette (m_HermesConverter,
								 m_HermesPalette, m_HermesPalette);
	}

#ifdef USE_VIDMODE
	if (m_Fullscreen)
	{
		if (m_BankSize >= m_MemSize)
		{
#ifdef USEASM
			if (m_Scale)
			{
				ExpandSetSurfaces (canvas->buffer,
			    	canvas->width, canvas->height, canvas->pitch,
					m_DisplayBase, m_DisplayPitch);
				ExpandCopy ();
			}
			else
#endif
				Hermes_ConverterCopy (m_HermesConverter, canvas->buffer,
					0, 0, canvas->width, canvas->height, canvas->pitch,
					m_DisplayBase, 0, 0, m_DisplayWidth, m_DisplayHeight,
					m_DisplayPitch);
		}
		else
		{
			int y1, y2, bank;
			int ysinc = m_DisplayRows >> (m_Scale ? 1 : 0);
			for (y1 = y2 = bank = 0;
				 y2 < m_DisplayHeight;
				 y1 += ysinc, y2 += m_DisplayRows, bank++)
			{
				XF86DGASetVidPage (m_Display, m_Screen, bank);
#ifdef USEASM
				if (m_Scale)
				{
					ExpandSetSurfaces (canvas->buffer + y2 * canvas->pitch,
									   canvas->width,
									   MIN (ysinc, m_DisplayHeight - y2),
									   canvas->pitch,
									   m_DisplayBase, m_DisplayPitch);
					ExpandCopy ();
				}
				else
#endif
					Hermes_ConverterCopy (m_HermesConverter,
										  canvas->buffer,
										  0, y1, canvas->width, ysinc,
										  canvas->pitch,
										  m_DisplayBase,
										  0, y2, m_DisplayWidth, m_DisplayRows,
										  m_DisplayPitch); 
			}
		}
	}
	else
#endif
	{
#ifdef USE_SHM
		while (!ShmFinished)
		{
			if (XPending (m_Display))
			{
				if (ActiveKeys)
					ActiveKeys->ProcessInput (true);
				else
				{
					XEvent ev;
					XNextEvent (m_Display, &ev);
					if (ev.type == m_ShmEventType)
						ShmFinished = true;
				}
			}
			else
			{
				usleep (5000);
			}
		}
#endif

#ifdef USEASM
		if (m_FastScale)
		{
			ExpandSetSurfaces (canvas->buffer,
							   canvas->width, canvas->height, canvas->pitch,
							   m_Image->DataPtr (), m_Image->Pitch ());
			ExpandCopy ();
		}
		else
#endif
			Hermes_ConverterCopy (m_HermesConverter,
				canvas->buffer, 0, 0,
				canvas->width, canvas->height, canvas->pitch,
				m_Image->DataPtr(), 0, 0,
				m_Image->Width(), m_Image->Height(),
				m_Image->Pitch());
		m_Image->Put (m_Window, m_GC);
	}

	if (m_NeedPalChange)
	{
		m_NeedPalChange--;
		if (m_Cmap)
		{
			int32 *palette = Hermes_PaletteGet (m_HermesPalette);
			int i;
			for (i = 0; i < 256; i++, palette++)
			{
				m_Colors[i].red = RPART(*palette) << 8;
				m_Colors[i].green = GPART(*palette) << 8;
				m_Colors[i].blue = BPART(*palette) << 8;
			}
			XStoreColors (m_Display, m_Cmap, m_Colors, 256);
		}
#ifdef USEASM
		else if (m_FastScale
#ifdef USE_VIDMODE
				|| (m_Fullscreen && m_Scale)
#endif
			   )
		{
			ExpandSetPalette (Hermes_PaletteGet (m_HermesPalette));
		}
#endif

	}
}

void XVideo::ReadScreen (byte *block)
{
	memcpy (block, m_LastUpdatedCanvas->m_Private,
			m_LastUpdatedCanvas->width * m_LastUpdatedCanvas->height);
}

bool XVideo::MakeModesList ()
{
#ifdef USE_VIDMODE
	if (m_HaveVidMode)
	{
		int nummodes, scaledmodes;
		int i, j, k;

		seteuid (0);
		if (geteuid())
		{
			ge->Printf (PRINT_HIGH,
						"hw_x: zdoom must be suid root to use VidMode\n");
			m_HaveVidMode = 0;
			return true;
		}

		scaledmodes = 0;
		XF86VidModeGetAllModeLines (m_Display, m_Screen,
									&nummodes, &m_VidModes);
		seteuid (getuid ());
		m_NumVidModes = nummodes;
		for (i = 0; i < nummodes; i++)
		{
			int twidth = m_VidModes[i]->hdisplay / 2;
			int theight = m_VidModes[i]->vdisplay / 2;
			if (twidth >= 320 && theight >= 200)
			{
				for (j = 0; j < nummodes; j++)
				{
					if (m_VidModes[j]->hdisplay == twidth &&
						m_VidModes[j]->vdisplay == theight)
					{
						break;
					}
				}
				if (j == nummodes)
					scaledmodes++;
			}
		}

		m_NumModes = nummodes + scaledmodes;
		m_Modes = new ModeInfo[m_NumModes];
		for (i = j = 0; i < nummodes && j < m_NumModes; i++)
		{
			int twidth = m_VidModes[i]->hdisplay;
			int theight = m_VidModes[i]->vdisplay;
			m_Modes[j].width = m_VidModes[i]->hdisplay;
			m_Modes[j].height = m_VidModes[i]->vdisplay;
			m_Modes[j].scaled = false;
			m_Modes[j].modeline = i;
			j++;
			twidth /= 2;
			theight /= 2;
			if (twidth >= 320 && theight >= 200)
			{
				for (k = 0; k < nummodes; k++)
				{
					if (m_VidModes[k]->hdisplay == twidth &&
						m_VidModes[k]->vdisplay == theight)
					{
						break;
					}
				}
				if (k == nummodes)
				{
					m_Modes[j].width = twidth;
					m_Modes[j].height = theight;
					m_Modes[j].scaled = true;
					m_Modes[j].modeline = i;
					j++;
				}
			}
		}
		qsort (m_Modes, m_NumModes, sizeof(ModeInfo), SortModes);
	}
	else
#endif
	{
		m_NumModes = 0;
	}

	return true;
}

int XVideo::SortModes (const void *a, const void *b)
{
	ModeInfo *mode1 = (ModeInfo *)a;
	ModeInfo *mode2 = (ModeInfo *)b;
	if (mode1->width < mode2->width)
		return -1;
	if (mode1->width > mode2->width)
		return 1;
	if (mode1->height < mode2->height)
		return -1;
	return 1;
}

void XVideo::FreeModes ()
{
#ifdef USE_VIDMODE
	if (m_VidModes)
		XFree (m_VidModes);
#endif
	if (m_Modes)
		delete[] m_Modes;
}

// This only changes how the iterator lists modes
bool XVideo::FullscreenChanged (bool fs)
{
#ifdef USE_VIDMODE
	if (m_NumModes && m_IteratorFS != fs)
	{
		m_IteratorFS = fs;
		return true;
	}
#endif
	return false;
}

int XVideo::GetModeCount ()
{
	int i, count;

#ifdef USE_VIDMODE
	if (m_IteratorFS)
	{
		return m_NumModes;
	}
	else
#endif
	{
		for (i = count = 0; m_WinModes[i].width; i++)
		{
			if (m_WinModes[i].width < m_DisplayWidth &&
				m_WinModes[i].height < m_DisplayHeight)
			{
				count++;
			}
		}
		return count;
	}
}

void XVideo::StartModeIterator (int bits)
{
	m_IteratorMode = 0;
	m_IteratorBits = bits;
}

bool XVideo::NextMode (int *width, int *height)
{
	if (m_IteratorBits != 8)
		return false;

#ifdef USE_VIDMODE
	if (m_IteratorFS)
	{
		if (m_IteratorMode < m_NumModes)
		{
			*width = m_Modes[m_IteratorMode].width;
			*height = m_Modes[m_IteratorMode].height;
			m_IteratorMode++;
			return true;
		}
	}
	else
#endif
	{
		for (; m_WinModes[m_IteratorMode].width; m_IteratorMode++)
		{
			if (m_WinModes[m_IteratorMode].width >= m_DisplayWidth ||
				m_WinModes[m_IteratorMode].height >= m_DisplayHeight)
			{
				continue;
			}
			*width = m_WinModes[m_IteratorMode].width;
			*height = m_WinModes[m_IteratorMode].height;
			m_IteratorMode++;
			return true;
		}
	}

    return false;
}

bool XVideo::AllocateSurface (DCanvas *scrn, int width, int height,
								 int bits, bool primary)
{
    if (scrn->m_Private)
		ReleaseSurface (scrn);

    scrn->width = width;
    scrn->height = height;
    scrn->is8bit = (bits == 8) ? true : false;
    scrn->bits = m_DisplayBits;
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

void XVideo::ReleaseSurface (DCanvas *scrn)
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

void XVideo::LockSurface (DCanvas *scrn)
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

void XVideo::UnlockSurface (DCanvas *scrn)
{
	if (scrn->m_Private)
	{
		scrn->buffer = NULL;
	}
}

bool XVideo::Blit (DCanvas *src, int sx, int sy, int sw, int sh,
					  DCanvas *dst, int dx, int dy, int dw, int dh)
{
	if (Hermes_ConverterRequest (m_HermesConverter, &m_SourceFormat, &m_SourceFormat))
	{
		Hermes_ConverterPalette (m_HermesConverter, m_HermesPalette, m_HermesPalette);
		Hermes_ConverterCopy (m_HermesConverter,
							  src->buffer, sx, sy, sw, sh, src->pitch,
							  dst->buffer, dx, dy, dw, dh, dst->pitch);
		return true;
	}
	else
		return false;
}

// KEYBOARD -----------------------------------------------------------------

byte XKeyboard::KPRemap[2][2][DIK_DECIMAL-DIK_NUMPAD7+1] =
{
	{ { DIK_7, DIK_8, DIK_9, DIK_SUBTRACT, DIK_4, DIK_5, DIK_6, DIK_ADD,
		DIK_1, DIK_2, DIK_3, DIK_0, DIK_PERIOD },
	  { '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.' } },

	{ { KEY_HOME, KEY_UPARROW, KEY_PGUP, DIK_SUBTRACT,
		KEY_LEFTARROW, DIK_NUMPAD5, KEY_RIGHTARROW, DIK_ADD,
		KEY_END, KEY_DOWNARROW, KEY_PGDN, KEY_INS, KEY_DEL },
	  { 0, 0, 0, '-', 0, '5', 0, '+', 0, 0, 0, 0, 0 } }
};

int XKeyboard::m_CompatXLateCount = 0;
XKeyboard::CompatMap XKeyboard::m_CompatXLate[] =
{
	{ XK_BackSpace,		{ "backspace" } },
	{ XK_Tab,			{ "tab" } },
	{ XK_Return,		{ "enter" } },
	{ XK_Pause,			{ "pause" } },
	{ XK_Scroll_Lock,	{ "scroll" } },
	{ XK_Sys_Req,		{ "sysrq" } },
	{ XK_Escape,		{ "escape" } },
	{ XK_Delete,		{ "del" } },
	{ XK_Kanji,			{ "kanji" } },
	{ XK_Home,			{ "home" } },
	{ XK_Left,			{ "leftarrow" } },
	{ XK_Up,			{ "uparrow" } },
	{ XK_Right,			{ "rightarrow" } },
	{ XK_Down,			{ "downarrow" } },
	{ XK_Prior,			{ "pgup" } },
	{ XK_Next,			{ "pgdn" } },
	{ XK_End,			{ "end" } },
	{ XK_Select,		{ "lwin" } },
	{ XK_Print,			{ "sysrq" } },
	{ XK_Insert,		{ "ins" } },
	{ XK_Find,			{ "pause" } },
	{ XK_Break,			{ "pause" } },
	{ XK_Mode_switch,	{ "alt" } },
	{ XK_Num_Lock,		{ "numlock" } },
	{ XK_KP_Enter,		{ "enter" } },
	{ XK_KP_Tab,		{ "tab" } },
	{ XK_KP_Space,		{ "space" } },
	{ XK_KP_F1,			{ "f1" } },
	{ XK_KP_F2,			{ "f2" } },
	{ XK_KP_F3,			{ "f3" } },
	{ XK_KP_F4,			{ "f4" } },
	{ XK_KP_Home,		{ "kp7" } },
	{ XK_KP_Left,		{ "kp4" } },
	{ XK_KP_Up,			{ "kp8" } },
	{ XK_KP_Right,		{ "kp6" } },
	{ XK_KP_Down,		{ "kp2" } },
	{ XK_KP_Prior,		{ "kp9" } },
	{ XK_KP_Next,		{ "kp3" } },
	{ XK_KP_End,		{ "kp1" } },
	{ XK_KP_Begin,		{ "kp7" } },
	{ XK_KP_Insert,		{ "kp0" } },
	{ XK_KP_Delete,		{ "kp." } },
	{ XK_KP_Equal,		{ "kp=" } },
	{ XK_KP_Multiply,	{ "kp*" } },
	{ XK_KP_Add,		{ "kp+" } },
	{ XK_KP_Separator,	{ "kp," } },
	{ XK_KP_Subtract,	{ "kp-" } },
	{ XK_KP_Decimal,	{ "kp." } },
	{ XK_KP_Divide,		{ "kp/" } },
	{ XK_KP_0,			{ "kp0" } },
	{ XK_KP_1,			{ "kp1" } },
	{ XK_KP_2,			{ "kp2" } },
	{ XK_KP_3,			{ "kp3" } },
	{ XK_KP_4,			{ "kp4" } },
	{ XK_KP_5,			{ "kp5" } },
	{ XK_KP_6,			{ "kp6" } },
	{ XK_KP_7,			{ "kp7" } },
	{ XK_KP_8,			{ "kp8" } },
	{ XK_KP_9,			{ "kp9" } },
	{ XK_F1,			{ "f1" } },
	{ XK_F2,			{ "f2" } },
	{ XK_F3,			{ "f3" } },
	{ XK_F4,			{ "f4" } },
	{ XK_F5,			{ "f5" } },
	{ XK_F6,			{ "f6" } },
	{ XK_F7,			{ "f7" } },
	{ XK_F8,			{ "f8" } },
	{ XK_F9,			{ "f9" } },
	{ XK_F10,			{ "f10" } },
	{ XK_F11,			{ "f11" } },
	{ XK_F12,			{ "f12" } },
	{ XK_F13,			{ "f13" } },
	{ XK_F14,			{ "f14" } },
	{ XK_F15,			{ "f15" } },
	{ XK_Shift_L,		{ "shift" } },
	{ XK_Shift_R,		{ "shift" } },
	{ XK_Control_L,		{ "ctrl" } },
	{ XK_Control_R,		{ "ctrl" } },
	{ XK_Caps_Lock,		{ "capslock" } },
	{ XK_Shift_Lock,	{ "capslock" } },
	{ XK_Meta_L,		{ "alt" } },
	{ XK_Meta_R,		{ "alt" } },
	{ XK_Alt_L,			{ "alt" } },
	{ XK_Alt_R,			{ "alt" } },
	{ XK_space,			{ "space" } },
	{ XK_plus,			{ "=" } },
	{ XK_comma,			{ "," } },
	{ XK_minus,			{ "-" } },
	{ XK_period,		{ "." } },
	{ XK_slash,			{ "/" } },
	{ XK_0, 			{ "0" } },
	{ XK_1, 		  	{ "1" } },
	{ XK_2, 		   	{ "2" } },
	{ XK_3, 		   	{ "3" } },
	{ XK_4, 		   	{ "4" } },
	{ XK_5, 		   	{ "5" } },
	{ XK_6, 		   	{ "6" } },
	{ XK_7, 		   	{ "7" } },
	{ XK_8, 		   	{ "8" } },
	{ XK_9, 		   	{ "9" } },
	{ XK_colon,			{ ":" } },
	{ XK_semicolon,		{ ";" } },
	{ XK_less,			{ "," } },
	{ XK_equal,			{ "=" } },
	{ XK_greater,		{ "." } },
	{ XK_question,		{ "/" } },
	{ XK_at,			{ "@" } },
	{ XK_A,				{ "a" } },
	{ XK_B,				{ "b" } },
	{ XK_C,				{ "c" } },
	{ XK_D,				{ "d" } },
	{ XK_E,				{ "e" } },
	{ XK_F,				{ "f" } },
	{ XK_G,				{ "g" } },
	{ XK_H,				{ "h" } },
	{ XK_I,				{ "i" } },
	{ XK_J,				{ "j" } },
	{ XK_K,				{ "k" } },
	{ XK_L,				{ "l" } },
	{ XK_M,				{ "m" } },
	{ XK_N,				{ "n" } },
	{ XK_O,				{ "o" } },
	{ XK_P,				{ "p" } },
	{ XK_Q,				{ "q" } },
	{ XK_R,				{ "r" } },
	{ XK_S,				{ "s" } },
	{ XK_T,				{ "t" } },
	{ XK_U,				{ "u" } },
	{ XK_V,				{ "v" } },
	{ XK_W,				{ "w" } },
	{ XK_X,				{ "x" } },
	{ XK_Y,				{ "y" } },
	{ XK_Z,				{ "z" } },
	{ XK_bracketleft,	{ "[" } },
	{ XK_backslash,		{ "\\" } },
	{ XK_bracketright,	{ "]" } },
	{ XK_underscore,	{ "_" } },
	{ XK_grave,			{ "`" } },
	{ XK_at,			{ "@" } },
	{ XK_a,				{ "a" } },
	{ XK_b,				{ "b" } },
	{ XK_c,				{ "c" } },
	{ XK_d,				{ "d" } },
	{ XK_e,				{ "e" } },
	{ XK_f,				{ "f" } },
	{ XK_g,				{ "g" } },
	{ XK_h,				{ "h" } },
	{ XK_i,				{ "i" } },
	{ XK_j,				{ "j" } },
	{ XK_k,				{ "k" } },
	{ XK_l,				{ "l" } },
	{ XK_m,				{ "m" } },
	{ XK_n,				{ "n" } },
	{ XK_o,				{ "o" } },
	{ XK_p,				{ "p" } },
	{ XK_q,				{ "q" } },
	{ XK_r,				{ "r" } },
	{ XK_s,				{ "s" } },
	{ XK_t,				{ "t" } },
	{ XK_u,				{ "u" } },
	{ XK_v,				{ "v" } },
	{ XK_w,				{ "w" } },
	{ XK_x,				{ "x" } },
	{ XK_y,				{ "y" } },
	{ XK_z,				{ "z" } },
	{ XK_braceleft,		{ "[" } },
	{ XK_bar,			{ "\\" } },
	{ XK_braceright,	{ "]" } },
	{ XK_yen,			{ "yen" } },
	{ XK_Henkan_Mode,	{ "convert" } },
	{ XK_Muhenkan,		{ "noconvert" } },
	{ XK_Cancel,		{ "stop" } },
	{ XK_Multi_key,		{ "apps" } },
#ifdef XK_ColonSign
	{ XK_ColonSign,		{ ":" } },
#endif
#ifdef XK_dead_circumflex
	{ XK_dead_circumflex, { "circumflex" } },
#endif
	{ 0, { NULL } }
};

XKeyboard::XKeyboard ()
{
	m_Inited = true;
	m_RemapKeypad = true;
	PrepCompatXLateKey ();
	LoadKeymap ();
	globalres = 0;
}

XKeyboard::~XKeyboard ()
{
	ActiveKeys = NULL;
    if (m_Inited)
    {
		m_Inited = false;
    }
}

void XKeyboard::LoadKeymap ()
{
	m_GotKeymap = false;
	char keyname[32];
	int code;

	memset (m_ToDICodes, 0, sizeof(m_ToDICodes));
	memset (m_ToANSI, 0, sizeof(m_ToANSI));

	FILE *f = fopen (KEYMAP_FILE, "r");

	if (f)
	{
		while (fscanf (f, "%u %31s\n", &code, keyname) == 2)
		{
			int i;
			for (i = 0; i < 256; i++)
			{
				if (KeyNames[i] && strcmp (KeyNames[i], keyname) == 0)
				{
					m_ToDICodes[code] = i;
					break;
				}
			}
			if (keyname[1] == 0)
				m_ToANSI[code] = keyname[0];
			else if (keyname[0] == 'k' &&
					 keyname[1] == 'p' &&
					 keyname[2] &&
					 keyname[3] == '\0')
				m_ToANSI[code] = keyname[2];
			else if (strcmp (keyname, "escape") == 0)
				m_ToANSI[code] = 27;
			else if (strcmp (keyname, "backscape") == 0)
				m_ToANSI[code] = 8;
			else if (strcmp (keyname, "tab") == 0)
				m_ToANSI[code] = 9;
			else if (strcmp (keyname, "enter") == 0)
				m_ToANSI[code] = 13;
		}
		fclose (f);
		m_GotKeymap = true;
	}
}

void XKeyboard::PrepCompatXLateKey ()
{
	if (m_CompatXLateCount > 0)
		return;

	int i, j;

	for (i = 0; m_CompatXLate[i].Sym; i++)
	{
		for (j = 0; j < 256; j++)
		{
			if (KeyNames[j] && 
				strcmp (KeyNames[j], m_CompatXLate[i].U.Label) == 0)
			{
				m_CompatXLate[i].U.DICode = j;
				break;
			}
		}
		if (j == 256)
		{
			ge->Printf (PRINT_HIGH, "hw_x: Unknown key '%s'\n",
						m_CompatXLate[i].U.Label);
			m_CompatXLate[i].U.DICode = 0;
		}
	}
	m_CompatXLateCount = i;
	qsort (m_CompatXLate, i, sizeof(CompatMap), Comparator); 
}

int XKeyboard::Comparator (const void *a, const void *b)
{
	return ((CompatMap *)a)->Sym - ((CompatMap *)b)->Sym;
}

int XKeyboard::CompatXLateKey (Display *disp, KeyCode code)
{
	KeySym sym = XKeycodeToKeysym (disp, code, 0);
	int low = 0;
	int high = m_CompatXLateCount - 1;

	while (low <= high)
	{
		int mid = (low + high) / 2;
		KeySym seesym = m_CompatXLate[mid].Sym;
		if (seesym == sym)
			return m_CompatXLate[mid].U.DICode;
		else if (sym < seesym)
			high = mid - 1;
		else
			low = mid + 1;
	}
	return 0;
}

void XKeyboard::FillKeycode (event_t &ev, KeyCode keycode)
{
	if (m_GotKeymap)
	{
		ev.data1 = m_ToDICodes[keycode];
		ev.data2 = m_ToANSI[keycode];
	}
	else
	{
		ev.data1 = CompatXLateKey (ActiveVideo->m_Display, keycode);
		switch (ev.data1)
		{
		case KEY_ESCAPE:	ev.data2 = 27;	break;
		case KEY_BACKSPACE:	ev.data2 = 8;	break;
		case KEY_TAB:		ev.data2 = 9;	break;
		case KEY_ENTER:		ev.data2 = 13;	break;
		default:
			if (KeyNames[ev.data1])
			{
				if (KeyNames[ev.data1][0] && KeyNames[ev.data1][1] == '\0')
					ev.data2 = KeyNames[ev.data1][0];
				else if (KeyNames[ev.data1][0] == 'k' &&
						 KeyNames[ev.data1][1] == 'p' &&
						 KeyNames[ev.data1][2] &&
						 KeyNames[ev.data1][3] == '\0')
					ev.data2 = KeyNames[ev.data1][2];
			}
			else
				ev.data2 = 0;
			break;
		}
	}
}

void XKeyboard::SetKeypadRemapping (bool remap)
{
	m_RemapKeypad = remap;
}

void XKeyboard::ProcessInput (bool consoleOpen)
{
	if (!ActiveVideo)
		return;

	XEvent xev;
	event_t ev;
	int mousex = 0, mousey = 0;

	while (XPending (ActiveVideo->m_Display))
	{
		XNextEvent (ActiveVideo->m_Display, &xev);
		switch (xev.type)
		{
		case ClientMessage:
			if ((Atom)xev.xclient.data.l[0] == ActiveVideo->m_DeleteAtom)
				exit (0);
			break;

		case KeyPress:
		case KeyRelease:
			ev.type = xev.type == KeyPress ? ev_keydown : ev_keyup;
			ev.data1 = 0;

			if (ev.type == ev_keydown)
				m_KeyState[xev.xkey.keycode/8] |= 1 << (xev.xkey.keycode&7);
			else
				m_KeyState[xev.xkey.keycode/8] &= ~(1 << (xev.xkey.keycode&7));

			FillKeycode (ev, xev.xkey.keycode);
			if (ev.data1)
			{
				int lookup = -1;

				if (ev.data1 >= DIK_NUMPAD7 && ev.data1 <= DIK_DECIMAL)
				{
					if (consoleOpen)
						lookup = 0;
					else if (m_RemapKeypad)
						lookup = 1;

					if (lookup >= 0)
					{
						ev.data2 = ev.data3 =
							KPRemap[lookup][1][ev.data1-DIK_NUMPAD7];
						ev.data1 = KPRemap[lookup][0][ev.data1-DIK_NUMPAD7];
					}
				}
				if (lookup < 0)
				{
					KeySym sym;
					if (XLookupString (&xev.xkey, NULL, 0, &sym, NULL))
					{
/******** WARNING: This is potentially non-portable *********/
						if ((sym & 0xff00) == 0)
							ev.data3 = sym & 0xff;
						else switch (sym)
						{
						case XK_BackSpace:	ev.data3 = 8;	break;
						case XK_Tab:		ev.data3 = 9;	break;
						case XK_KP_Enter:
						case XK_Return:		ev.data3 = 13;	break;
						case XK_Escape:		ev.data3 = 27;	break;
						default:
							ev.data3 = 0;
						}
					}
				}
				ge->PostEvent (&ev);
			}
			break;

		case FocusIn:
			m_Focused = true;
			break;

		case FocusOut:
			m_Focused = false;
			{
				ev.type = ev_keyup;
				ev.data3 = 0;
				int i;
				for (i = 0; i < 256/8; i++)
				{
					byte j = m_KeyState[i];
					int k = 0;
					while (j)
					{
						if (j & 1)
						{
							FillKeycode (ev, i*8+k);
							if (ev.data1)
								ge->PostEvent (&ev);
							m_KeyState[i] &= ~(1<<k);
						}
						j >>= 1;
					}
				}
			}
			break;

#ifdef USE_DGA
		// Ouch! Mouse code doesn't belong in our keyboard class!
		case ButtonPress:
			if (ActiveVideo->m_HaveDGA && ActiveMouse)
			{
				ev.type = ev_keydown;
				ev.data2 = ev.data3 = 0;
				switch (xev.xbutton.button)
				{
				case 1:		ev.data1 = KEY_MOUSE1;		break;
				case 2:		ev.data1 = KEY_MOUSE3;		break;
				case 3:		ev.data1 = KEY_MOUSE2;		break;
				case 4:		ev.data1 = KEY_MWHEELUP;	break;
				case 5:		ev.data1 = KEY_MWHEELDOWN;	break;
				default:	ev.data1 = 0;				break;
				}
				if (ev.data1)
					ge->PostEvent (&ev);
			}
			break;

		case ButtonRelease:
			if (ActiveVideo->m_HaveDGA && ActiveMouse)
			{
				ev.type = ev_keyup;
				ev.data2 = ev.data3 = 0;
				switch (xev.xbutton.button)
				{
				case 1:		ev.data1 = KEY_MOUSE1;		break;
				case 2:		ev.data1 = KEY_MOUSE3;		break;
				case 3:		ev.data1 = KEY_MOUSE2;		break;
				case 4:		ev.data1 = KEY_MWHEELUP;	break;
				case 5:		ev.data1 = KEY_MWHEELDOWN;	break;
				default:	ev.data1 = 0;				break;
				}
				if (ev.data1)
					ge->PostEvent (&ev);
			}
			break;

		case MotionNotify:
			if (ActiveVideo->m_HaveDGA && ActiveMouse
				&& ActiveMouse->m_IsGrabbed)
			{
				if (ActiveMouse->m_JustGrabbed)
				{
					ActiveMouse->m_JustGrabbed = false;
				}
				else
				{
				    mousex += xev.xmotion.x_root;
					mousey += xev.xmotion.y_root;
				}
			}
			break;
#endif

		default:
#ifdef USE_SHM
			if (ActiveVideo->m_DoShm &&
				xev.type == ActiveVideo->m_ShmEventType)
			{
				ShmFinished = true;
			}
#endif
			break;
		}
	}

	if (mousex | mousey)
	{
		ev.type = ev_mouse;
		ev.data1 = 0;
		ev.data2 = mousex * 4;
		ev.data3 = mousey * -2;
		ge->PostEvent (&ev);
	}
}

// MOUSE ----------------------------------------------------------------------

XMouse::XMouse ()
{
	m_ShouldGrab = m_IsGrabbed = m_JustGrabbed = false;
}

XMouse::~XMouse ()
{
	ActiveMouse = NULL;
	DoGrab (false);
}

void XMouse::ProcessInput (bool active)
{
	if (!ActiveVideo)
		return;

	if (!active)
	{
		DoGrab (false);
		return;
	}

	bool wasgrabbed = m_IsGrabbed;
	DoGrab (m_ShouldGrab);

	if (m_IsGrabbed
#ifdef USE_DGA
		&& !ActiveVideo->m_HaveDGA
#endif
		)
	{
		event_t ev;
		bool recenter = !wasgrabbed;

		if (!recenter)
		{
			int rootx, rooty;
			int winx, winy;
			Window root, child;
			unsigned int buttons;

			if (!XQueryPointer (ActiveVideo->m_Display,
								ActiveVideo->m_Window,
								&root, &child,
								&rootx, &rooty,
								&winx, &winy,
								&buttons))
				return;

			ev.type = ev_mouse;
			ev.data1 = 0;
			ev.data2 = (winx - m_LastX) * 5;
			ev.data3 = (m_LastY - winy) * 3;
			if ( (recenter = ev.data2 | ev.data3) )
				ge->PostEvent (&ev);

			ev.data2 = ev.data3 = 0;
			if ((m_LastButtons & Button1Mask) ^ (buttons & Button1Mask))
			{
				ev.data1 = KEY_MOUSE1;
				ev.type = (buttons & Button1Mask) ? ev_keydown : ev_keyup;
				ge->PostEvent (&ev);
			}
			if ((m_LastButtons & Button2Mask) ^ (buttons & Button2Mask))
			{
				ev.data1 = KEY_MOUSE3;
				ev.type = (buttons & Button2Mask) ? ev_keydown : ev_keyup;
				ge->PostEvent (&ev);
			}
			if ((m_LastButtons & Button3Mask) ^ (buttons & Button3Mask))
			{
				ev.data1 = KEY_MOUSE2;
				ev.type = (buttons & Button3Mask) ? ev_keydown : ev_keyup;
				ge->PostEvent (&ev);
			}
			if ((m_LastButtons & Button4Mask) ^ (buttons & Button4Mask))
			{
				ev.data1 = KEY_MWHEELUP;
				ev.type = (buttons & Button4Mask) ? ev_keydown : ev_keyup;
				ge->PostEvent (&ev);
			}
			if ((m_LastButtons & Button5Mask) ^ (buttons & Button5Mask))
			{
				ev.data1 = KEY_MWHEELDOWN;
				ev.type = (buttons & Button5Mask) ? ev_keydown : ev_keyup;
				ge->PostEvent (&ev);
			}
			m_LastButtons = buttons;
		}

		if (recenter)
		{
			m_LastX = ActiveVideo->m_Image->Width() / 2;
			m_LastY = ActiveVideo->m_Image->Height() / 2;
			XWarpPointer (ActiveVideo->m_Display, None,
						  ActiveVideo->m_Window, 0, 0, 0, 0,
						  m_LastX, m_LastY);
		}
	}
}

void XMouse::SetGrabbed (bool grabbed)
{
	m_ShouldGrab = grabbed;
}

void XMouse::DoGrab (bool grabbed)
{
#ifdef USE_VIDMODE
	if (ActiveVideo->m_Fullscreen)
	{
		m_IsGrabbed = true;
		return;
	}
#endif

	if (grabbed && ActiveKeys && !ActiveKeys->m_Focused)
		grabbed = false;

	if (grabbed == m_IsGrabbed)
		return;

	if (!(m_IsGrabbed = grabbed))
	{
#ifdef USE_DGA
		if (ActiveVideo->m_HaveDGA)
			XF86DGADirectVideo (ActiveVideo->m_Display,
								ActiveVideo->m_Screen,
								0);
		else
#endif
			XUngrabPointer (ActiveVideo->m_Display, CurrentTime);
		ActiveVideo->ShowCursor ();
	}
	else
	{
#ifdef USE_DGA
		if (ActiveVideo->m_HaveDGA)
		{
			m_LastX = ActiveVideo->m_Image->Width() / 2;
			m_LastY = ActiveVideo->m_Image->Height() / 2;
			XWarpPointer (ActiveVideo->m_Display, None,
						  ActiveVideo->m_Window, 0, 0, 0, 0,
						  m_LastX, m_LastY);
			XF86DGADirectVideo (ActiveVideo->m_Display,
								ActiveVideo->m_Screen,
								XF86DGADirectMouse);
			m_JustGrabbed = true;
		}
		else
#endif
			XGrabPointer (ActiveVideo->m_Display,
						  ActiveVideo->m_Window, True, 0,
						  GrabModeAsync, GrabModeAsync,
						  ActiveVideo->m_Window, None, CurrentTime);
		ActiveVideo->HideCursor ();
	}
}

// JOYSTICK -------------------------------------------------------------------

class XJoystick : public IJoystick
{
public:
	XJoystick (int whichjoy);
	~XJoystick ();
	void ProcessInput (bool active);
	void SetProperty (EJoyProp prop, float val);
};

void XJoystick::SetProperty (EJoyProp prop, float val)
{
}

XJoystick::XJoystick (int whichjoy)
{
	globalres = 1;
}

XJoystick::~XJoystick ()
{
}

void XJoystick::ProcessInput (bool active)
{
}

// ----------------------------------------------------------------------------

static void *QueryInterface (EInterfaceType type, int parm)
{
	void *res;

	globalres = 0;
	switch (type)
	{
	case INTERFACE_Video:		res = ActiveVideo = new XVideo;	break;
	case INTERFACE_Keyboard:	res = ActiveKeys = new XKeyboard;	break;
	case INTERFACE_Mouse:		res = ActiveMouse = new XMouse;	break;
	case INTERFACE_Joystick:	res = new XJoystick (parm);		break;
	default:					return NULL;
	}
	if (res == NULL)
		return NULL;
	if (globalres == 0)
		return res;
	switch (type)
	{
	case INTERFACE_Video:		delete (XVideo *)res;			break;
	case INTERFACE_Keyboard:	delete (XKeyboard *)res;		break;
	case INTERFACE_Mouse:		delete (XMouse *)res;			break;
	case INTERFACE_Joystick:	delete (XJoystick *)res;		break;
	}
	return NULL;
}

extern "C" queryinterface_t PrepLibrary (IEngineGlobals *globs)
{
	ge = globs;
	return QueryInterface;
}
