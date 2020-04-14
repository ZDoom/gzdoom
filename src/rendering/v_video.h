/*
** v_video.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __V_VIDEO_H__
#define __V_VIDEO_H__

#include <functional>
#include "doomtype.h"
#include "vectors.h"

#include "doomdef.h"
#include "m_png.h"
#include "dobject.h"
#include "renderstyle.h"
#include "c_cvars.h"
#include "v_colortables.h"
#include "v_2ddrawer.h"

#include "hwrenderer/dynlights/hw_shadowmap.h"

static const int VID_MIN_WIDTH = 320;
static const int VID_MIN_HEIGHT = 200;

static const int VID_MIN_UI_WIDTH = 640;
static const int VID_MIN_UI_HEIGHT = 400;

class player_t;
struct sector_t;
struct FPortalSceneState;
class FSkyVertexBuffer;
class IIndexBuffer;
class IVertexBuffer;
class IDataBuffer;
class FFlatVertexBuffer;
class HWViewpointBuffer;
class FLightBuffer;
struct HWDrawInfo;

enum EHWCaps
{
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION = 1,
	RFL_TEXTURE_COMPRESSION_S3TC = 2,

	RFL_SHADER_STORAGE_BUFFER = 4,
	RFL_BUFFER_STORAGE = 8,

	RFL_NO_CLIP_PLANES = 32,

	RFL_INVALIDATE_BUFFER = 64,
	RFL_DEBUG = 128,
};


struct IntRect
{
	int left, top;
	int width, height;


	void Offset(int xofs, int yofs)
	{
		left += xofs;
		top += yofs;
	}

	void AddToRect(int x, int y)
	{
		if (x < left)
			left = x;
		if (x > left + width)
			width = x - left;

		if (y < top)
			top = y;
		if (y > top + height)
			height = y - top;
	}


};


extern int DisplayWidth, DisplayHeight;

void V_UpdateModeSize (int width, int height);
void V_OutputResized (int width, int height);
void V_CalcCleanFacs (int designwidth, int designheight, int realwidth, int realheight, int *cleanx, int *cleany, int *cx1=NULL, int *cx2=NULL);

EXTERN_CVAR(Int, vid_rendermode)
EXTERN_CVAR(Bool, vid_fullscreen)
EXTERN_CVAR(Int, win_x)
EXTERN_CVAR(Int, win_y)
EXTERN_CVAR(Int, win_w)
EXTERN_CVAR(Int, win_h)
EXTERN_CVAR(Bool, win_maximized)


inline bool V_IsHardwareRenderer()
{
	return vid_rendermode == 4;
}

inline bool V_IsSoftwareRenderer()
{
	return vid_rendermode < 2;
}

inline bool V_IsPolyRenderer()
{
	return vid_rendermode == 2 || vid_rendermode == 3;
}

inline bool V_IsTrueColor()
{
	return vid_rendermode == 1 || vid_rendermode == 3 || vid_rendermode == 4;
}


struct FColormap;
class FileWriter;
enum FTextureFormat : uint32_t;
class FModelRenderer;
struct SamplerUniform;

//
// VIDEO
//
//
class DCanvas
{
public:
	DCanvas (int width, int height, bool bgra);
	~DCanvas ();
	void Resize(int width, int height, bool optimizepitch = true);

	// Member variable access
	inline uint8_t *GetPixels () const { return Pixels.Data(); }
	inline int GetWidth () const { return Width; }
	inline int GetHeight () const { return Height; }
	inline int GetPitch () const { return Pitch; }
	inline bool IsBgra() const { return Bgra; }

protected:
	TArray<uint8_t> Pixels;
	int Width;
	int Height;
	int Pitch;
	bool Bgra;
};

class IHardwareTexture;
class FTexture;


class DFrameBuffer
{
public:

	F2DDrawer m2DDrawer;
private:
	int Width = 0;
	int Height = 0;
public:
	//int clipleft = 0, cliptop = 0, clipwidth = -1, clipheight = -1;

public:
	// Hardware render state that needs to be exposed to the API independent part of the renderer. For ease of access this is stored in the base class.
	int hwcaps = 0;								// Capability flags
	float glslversion = 0;						// This is here so that the differences between old OpenGL and new OpenGL/Vulkan can be handled by platform independent code.
	int instack[2] = { 0,0 };					// this is globally maintained state for portal recursion avoidance.
	int stencilValue = 0;						// Global stencil test value
	unsigned int uniformblockalignment = 256;	// Hardware dependent uniform buffer alignment.
	unsigned int maxuniformblock = 65536;
	const char *vendorstring;					// We have to account for some issues with particular vendors.
	FPortalSceneState *mPortalState;			// global portal state.
	FSkyVertexBuffer *mSkyData = nullptr;		// the sky vertex buffer
	FFlatVertexBuffer *mVertexData = nullptr;	// Global vertex data
	HWViewpointBuffer *mViewpoints = nullptr;	// Viewpoint render data.
	FLightBuffer *mLights = nullptr;			// Dynamic lights
	IShadowMap mShadowMap;

	IntRect mScreenViewport;
	IntRect mSceneViewport;
	IntRect mOutputLetterbox;
	float mSceneClearColor[4];


public:
	DFrameBuffer (int width=1, int height=1);
	virtual ~DFrameBuffer();
	virtual void InitializeState() = 0;	// For stuff that needs 'screen' set.
	virtual bool IsVulkan() { return false; }
	virtual bool IsPoly() { return false; }

	virtual DCanvas* GetCanvas() { return nullptr; }

	void SetSize(int width, int height);
	void SetVirtualSize(int width, int height)
	{
		Width = width;
		Height = height;
	}
	inline int GetWidth() const { return Width; }
	inline int GetHeight() const { return Height; }

	FVector2 SceneScale() const
	{
		return { mSceneViewport.width / (float)mScreenViewport.width, mSceneViewport.height / (float)mScreenViewport.height };
	}

	FVector2 SceneOffset() const
	{
		return { mSceneViewport.left / (float)mScreenViewport.width, mSceneViewport.top / (float)mScreenViewport.height };
	}

	// Make the surface visible.
	virtual void Update ();

	// Stores the palette with flash blended in into 256 dwords
	// Mark the palette as changed. It will be updated on the next Update().
	virtual void UpdatePalette() {}

	virtual void SetGamma() {}

	// Returns true if running fullscreen.
	virtual bool IsFullscreen () = 0;
	virtual void ToggleFullscreen(bool yes) {}

	// Changes the vsync setting, if supported by the device.
	virtual void SetVSync (bool vsync);

	// Delete any resources that need to be deleted after restarting with a different IWAD
	virtual void CleanForRestart() {}
	virtual void SetTextureFilterMode() {}
	virtual IHardwareTexture *CreateHardwareTexture() { return nullptr; }
	virtual void PrecacheMaterial(FMaterial *mat, int translation) {}
	virtual FModelRenderer *CreateModelRenderer(int mli) { return nullptr; }
	virtual void TextureFilterChanged() {}
	virtual void BeginFrame() {}
	virtual void SetWindowSize(int w, int h) {}
	virtual void StartPrecaching() {}

	virtual int GetClientWidth() = 0;
	virtual int GetClientHeight() = 0;
	virtual void BlurScene(float amount) {}
    
    // Interface to hardware rendering resources
	virtual IVertexBuffer *CreateVertexBuffer() { return nullptr; }
	virtual IIndexBuffer *CreateIndexBuffer() { return nullptr; }
	virtual IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize) { return nullptr; }
	bool BuffersArePersistent() { return !!(hwcaps & RFL_BUFFER_STORAGE); }

	// Begin/End 2D drawing operations.
	void Begin2D() 
	{ 
		m2DDrawer.Begin();
		m2DDrawer.SetSize(Width, Height);
	}
	void End2D() { m2DDrawer.End(); }

	void End2DAndUpdate()
	{
		DrawRateStuff();
		m2DDrawer.End();
		Update();
	}

	// This is overridable in case Vulkan does it differently.
	virtual bool RenderTextureIsFlipped() const
	{
		return true;
	}

	// Report a game restart
	void SetClearColor(int color);
	virtual uint32_t GetCaps();
	virtual int Backend() { return 0; }
	virtual const char* DeviceName() const { return "Unknown"; }
	virtual void WriteSavePic(player_t *player, FileWriter *file, int width, int height);
	virtual sector_t *RenderView(player_t *player) { return nullptr;  }

	// Screen wiping
	virtual FTexture *WipeStartScreen();
	virtual FTexture *WipeEndScreen();

	virtual void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D) { if (afterBloomDrawEndScene2D) afterBloomDrawEndScene2D(); }

	void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

	uint64_t GetLastFPS() const { return LastCount; }

	virtual void Draw2D() {}
	void Clear2D() { m2DDrawer.Clear(); }

	// Calculate gamma table
	void CalcGamma(float gamma, uint8_t gammalookup[256]);

	virtual void SetViewportRects(IntRect *bounds);
	int ScreenToWindowX(int x);
	int ScreenToWindowY(int y);

	void FPSLimit();

	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual TArray<uint8_t> GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma) { return TArray<uint8_t>(); }

	static float GetZNear() { return 5.f; }
	static float GetZFar() { return 65536.f; }

	// The original size of the framebuffer as selected in the video menu.
	uint64_t FrameTime = 0;

protected:
	void DrawRateStuff ();

private:
	uint64_t fpsLimitTime = 0;

	uint64_t LastMS = 0, LastSec = 0, FrameCount = 0, LastCount = 0, LastTic = 0;

	bool isIn2D = false;
};


// This is the screen updated by I_FinishUpdate.
extern DFrameBuffer *screen;

#define SCREENWIDTH (screen->GetWidth ())
#define SCREENHEIGHT (screen->GetHeight ())
#define SCREENPITCH (screen->GetPitch ())

EXTERN_CVAR (Float, Gamma)


// Allocates buffer screens, call before R_Init.
void V_InitScreenSize();
void V_InitScreen();

// Initializes graphics mode for the first time.
void V_Init2 ();

void V_Shutdown ();

class FScanner;
struct FScriptPosition;

inline bool IsRatioWidescreen(int ratio) { return (ratio & 3) != 0; }


#include "v_draw.h"

class ScaleOverrider
{
	int savedxfac, savedyfac, savedwidth, savedheight;

public:
	// This is to allow certain elements to use an optimal fullscreen scale which for the menu would be too large.
	// The old code contained far too much mess to compensate for the menus which negatively affected everything else.
	// However, for compatibility reasons the currently used variables cannot be changed so they have to be overridden temporarily.
	// This class provides a safe interface for this because it ensures that the values get restored afterward.
	// Currently, the intermission and the level summary screen use this.
	ScaleOverrider()
	{
		savedxfac = CleanXfac;
		savedyfac = CleanYfac;
		savedwidth = CleanWidth;
		savedheight = CleanHeight;

		if (screen)
		{
			V_CalcCleanFacs(320, 200, screen->GetWidth(), screen->GetHeight(), &CleanXfac, &CleanYfac);
			CleanWidth = screen->GetWidth() / CleanXfac;
			CleanHeight = screen->GetHeight() / CleanYfac;
		}
	}

	~ScaleOverrider()
	{
		CleanXfac = savedxfac;
		CleanYfac = savedyfac;
		CleanWidth = savedwidth;
		CleanHeight = savedheight;
	}
};


#endif // __V_VIDEO_H__
