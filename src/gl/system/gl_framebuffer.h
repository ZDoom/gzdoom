#ifndef __GL_FRAMEBUFFER
#define __GL_FRAMEBUFFER

#ifdef _WIN32
#include "win32iface.h"
#include "win32gliface.h"
#endif

#include <memory>

class FHardwareTexture;
class FSimpleVertexBuffer;
class FGLDebug;

#ifdef _WIN32
class OpenGLFrameBuffer : public Win32GLFrameBuffer
{
	typedef Win32GLFrameBuffer Super;
#else
#include "sdlglvideo.h"
class OpenGLFrameBuffer : public SDLGLFB
{
	typedef SDLGLFB Super;	//[C]commented, DECLARE_CLASS defines this in linux
#endif


public:

	explicit OpenGLFrameBuffer() {}
	OpenGLFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen) ;
	~OpenGLFrameBuffer();

	void InitializeState();
	void Update();

	// Color correction
	bool SetGamma (float gamma);
	bool SetBrightness(float bright);
	bool SetContrast(float contrast);
	void DoSetGamma();

	void UpdatePalette();
	void GetFlashedPalette (PalEntry pal[256]);
	PalEntry *GetPalette ();
	bool SetFlash(PalEntry rgb, int amount);
	void GetFlash(PalEntry &rgb, int &amount);
	int GetPageCount();
	bool Begin2D(bool copy3d);
	void GameRestart();

	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual void GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type);

	// Releases the screenshot buffer.
	virtual void ReleaseScreenshotBuffer();

	// 2D drawing
	void DrawTextureParms(FTexture *img, DrawParms &parms);
	void DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32_t color);
	void DrawPixel(int x1, int y1, int palcolor, uint32_t color);
	void DoClear(int left, int top, int right, int bottom, int palcolor, uint32_t color);
	void Dim(PalEntry color=0);
	void DoDim (PalEntry color, float damount, int x1, int y1, int w, int h);
	void FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin=false);

	void FillSimplePoly(FTexture *tex, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip);

	FNativePalette *CreatePalette(FRemapTable *remap);

	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();
	void Swap();
	bool Is8BitMode() { return false; }
	bool IsHWGammaActive() const { return HWGammaActive; }

	void SetVSync(bool vsync);

	void ScaleCoordsFromWindow(int16_t &x, int16_t &y) override;

	bool HWGammaActive = false;			// Are we using hardware or software gamma?
	std::shared_ptr<FGLDebug> mDebug;	// Debug API
private:
	PalEntry Flash;						// Only needed to support some cruft in the interface that only makes sense for the software renderer
	PalEntry SourcePalette[256];		// This is where unpaletted textures get their palette from
	uint8_t *ScreenshotBuffer;			// What the name says. This must be maintained because the software renderer can return a locked canvas surface which the caller cannot release.

	class Wiper
	{

	protected:
		FSimpleVertexBuffer *mVertexBuf;

		void MakeVBO(OpenGLFrameBuffer *fb);

	public:
		Wiper();
		virtual ~Wiper();
		virtual bool Run(int ticks, OpenGLFrameBuffer *fb) = 0;
	};

	class Wiper_Melt;			friend class Wiper_Melt;
	class Wiper_Burn;			friend class Wiper_Burn;
	class Wiper_Crossfade;		friend class Wiper_Crossfade;

	Wiper *ScreenWipe;
	FHardwareTexture *wipestartscreen;
	FHardwareTexture *wipeendscreen;


public:
};


#endif //__GL_FRAMEBUFFER
