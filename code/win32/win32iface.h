#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

#include "hardware.h"
#include "v_video.h"

class Win32Video : public IVideo
{
 public:
	Win32Video (int parm);
	~Win32Video ();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	bool FullscreenChanged (bool fs);
	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool fs, DFrameBuffer *old);

	int GetModeCount ();
	void StartModeIterator (int bits);
	bool NextMode (int *width, int *height);

	void GoFullscreen (bool yes);

 private:
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

	ModeInfo *m_IteratorMode;
	int m_IteratorBits;
	bool m_IteratorFS;
	bool m_IsFullscreen;

	int m_DisplayWidth;
	int m_DisplayHeight;
	int m_DisplayBits;

	bool m_Have320x200x8;
	bool m_Have320x240x8;

	bool m_CalledCoInitialize;

	void AddMode (int x, int y, int bits, ModeInfo **lastmode);
	void FreeModes ();

	void InitDDraw ();
	void NewDDMode (int x, int y);
	struct CBData
	{
		Win32Video *self;
		ModeInfo *modes;
	};
	static HRESULT WINAPI EnumDDModesCB (LPDDSURFACEDESC desc, void *modes);
};
