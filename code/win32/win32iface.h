#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

#include "hardware.h"
#include "v_video.h"
#include "ptc.h"

class Win32Video : public IVideo
{
 public:
	Win32Video (int parm);
	~Win32Video ();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	bool FullscreenChanged (bool fs);
	void SetWindowedScale (float scale);
	bool CanBlit () { return false; }

	bool SetMode (int width, int height, int bits, bool fs);
	void SetPalette (DWORD *palette);
	void UpdateScreen (DCanvas *canvas);
	void ReadScreen (byte *block);

	int GetModeCount ();
	void StartModeIterator (int bits);
	bool NextMode (int *width, int *height);

	bool AllocateSurface (DCanvas *scrn, int width, int height, int bits, bool primary = false);
	void ReleaseSurface (DCanvas *scrn);
	void LockSurface (DCanvas *scrn);
	void UnlockSurface (DCanvas *scrn);
	bool Blit (DCanvas *src, int sx, int sy, int sw, int sh,
			   DCanvas *dst, int dx, int dy, int dw, int dh);

 private:
	struct Chain
	{
		Chain (DCanvas *inCanvas)
			: canvas (inCanvas)
		{}

		DCanvas *canvas;
		Chain *next;
	} *m_Chain;

	struct ModeInfo
	{
		ModeInfo (int inX, int inY, int inBits)
			: next (NULL),
			  width (inX),
			  height (inY),
			  bits (inBits)
		{}

		ModeInfo *next;
		int width, height, bits;
	} *m_Modes;

	union
	{
		PALETTEENTRY pe[256];
		int32		 ui[256];
	} m_PalEntries;

	ModeInfo *m_IteratorMode;
	int m_IteratorBits;
	bool m_IteratorFS;

	int m_DisplayWidth;
	int m_DisplayHeight;
	int m_DisplayBits;

	Console *m_Con;
	Palette *m_DisPal;

	DCanvas *m_LastUpdatedCanvas;

	IDirectDraw *m_DDObj;
	LPDIRECTDRAWPALETTE m_DDPalette;
	LPDIRECTDRAWSURFACE m_DDPrimary;
	LPDIRECTDRAWSURFACE m_DDBack;

	bool m_Have320x200x8;
	bool m_Have320x240x8;

	int m_NeedPalChange;
	bool m_CalledCoInitialize;

	void AddMode (int x, int y, int bits, ModeInfo **lastmode);
	void FreeModes ();
	void MakeModesList ();

	void InitDDraw ();
	void NewDDMode (int x, int y);
	struct CBData
	{
		Win32Video *self;
		ModeInfo *modes;
	};
	static HRESULT WINAPI EnumDDModesCB (LPDDSURFACEDESC desc, void *modes);
};
