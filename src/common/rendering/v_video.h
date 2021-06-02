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
#include "basics.h"
#include "vectors.h"
#include "m_png.h"
#include "renderstyle.h"
#include "c_cvars.h"
#include "v_2ddrawer.h"
#include "intrect.h"
#include "hw_shadowmap.h"


struct FPortalSceneState;
class FSkyVertexBuffer;
class IIndexBuffer;
class IVertexBuffer;
class IDataBuffer;
class FFlatVertexBuffer;
class HWViewpointBuffer;
class FLightBuffer;
struct HWDrawInfo;
class FMaterial;
class FGameTexture;
class FRenderState;

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


extern int DisplayWidth, DisplayHeight;

void V_UpdateModeSize (int width, int height);
void V_OutputResized (int width, int height);

EXTERN_CVAR(Bool, vid_fullscreen)
EXTERN_CVAR(Int, win_x)
EXTERN_CVAR(Int, win_y)
EXTERN_CVAR(Int, win_w)
EXTERN_CVAR(Int, win_h)
EXTERN_CVAR(Bool, win_maximized)

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
private:
	int Width = 0;
	int Height = 0;

public:
	// Hardware render state that needs to be exposed to the API independent part of the renderer. For ease of access this is stored in the base class.
	int hwcaps = 0;								// Capability flags
	float glslversion = 0;						// This is here so that the differences between old OpenGL and new OpenGL/Vulkan can be handled by platform independent code.
	int instack[2] = { 0,0 };					// this is globally maintained state for portal recursion avoidance.
	int stencilValue = 0;						// Global stencil test value
	unsigned int uniformblockalignment = 256;	// Hardware dependent uniform buffer alignment.
	unsigned int maxuniformblock = 65536;
	const char *vendorstring;					// We have to account for some issues with particular vendors.
	FSkyVertexBuffer *mSkyData = nullptr;		// the sky vertex buffer
	FFlatVertexBuffer *mVertexData = nullptr;	// Global vertex data
	HWViewpointBuffer *mViewpoints = nullptr;	// Viewpoint render data.
	FLightBuffer *mLights = nullptr;			// Dynamic lights
	IShadowMap mShadowMap;

	IntRect mScreenViewport;
	IntRect mSceneViewport;
	IntRect mOutputLetterbox;
	float mSceneClearColor[4]{ 0,0,0,255 };

public:
	DFrameBuffer (int width=1, int height=1);
	virtual ~DFrameBuffer();
	virtual void InitializeState() = 0;	// For stuff that needs 'screen' set.
	virtual bool IsVulkan() { return false; }
	virtual bool IsPoly() { return false; }
	void SetAABBTree(hwrenderer::LevelAABBTree * tree)
	{
		mShadowMap.SetAABBTree(tree);
	}

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

	// Returns true if running fullscreen.
	virtual bool IsFullscreen () = 0;
	virtual void ToggleFullscreen(bool yes) {}

	// Changes the vsync setting, if supported by the device.
	virtual void SetVSync (bool vsync);

	// Delete any resources that need to be deleted after restarting with a different IWAD
	virtual void SetTextureFilterMode() {}
	virtual IHardwareTexture *CreateHardwareTexture(int numchannels) { return nullptr; }
	virtual void PrecacheMaterial(FMaterial *mat, int translation) {}
	virtual FMaterial* CreateMaterial(FGameTexture* tex, int scaleflags);
	virtual void BeginFrame() {}
	virtual void SetWindowSize(int w, int h) {}
	virtual void StartPrecaching() {}
	virtual FRenderState* RenderState() { return nullptr; }

	virtual int GetClientWidth() = 0;
	virtual int GetClientHeight() = 0;
	virtual void BlurScene(float amount) {}
    
    // Interface to hardware rendering resources
	virtual IVertexBuffer *CreateVertexBuffer() { return nullptr; }
	virtual IIndexBuffer *CreateIndexBuffer() { return nullptr; }
	virtual IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize) { return nullptr; }
	bool BuffersArePersistent() { return !!(hwcaps & RFL_BUFFER_STORAGE); }

	// This is overridable in case Vulkan does it differently.
	virtual bool RenderTextureIsFlipped() const
	{
		return true;
	}

	// Report a game restart
	void SetClearColor(int color);
	virtual int Backend() { return 0; }
	virtual const char* DeviceName() const { return "Unknown"; }
	virtual void AmbientOccludeScene(float m5) {}
	virtual void FirstEye() {}
	virtual void NextEye(int eyecount) {}
	virtual void SetSceneRenderTarget(bool useSSAO) {}
	virtual void UpdateShadowMap() {}
	virtual void WaitForCommands(bool finish) {}
	virtual void SetSaveBuffers(bool yes) {}
	virtual void ImageTransitionScene(bool unknown) {}
	virtual void CopyScreenToBuffer(int width, int height, uint8_t* buffer)	{ memset(buffer, 0, width* height); }
	virtual bool FlipSavePic() const { return false; }
	virtual void RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect&)> renderFunc) {}
	virtual void SetActiveRenderTarget() {}

	// Screen wiping
	virtual FTexture *WipeStartScreen();
	virtual FTexture *WipeEndScreen();

	virtual void PostProcessScene(bool swscene, int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D) { if (afterBloomDrawEndScene2D) afterBloomDrawEndScene2D(); }

	void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

	virtual void Draw2D() {}

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

private:
	uint64_t fpsLimitTime = 0;

	bool isIn2D = false;
};


// This is the screen updated by I_FinishUpdate.
extern DFrameBuffer *screen;

#define SCREENWIDTH (screen->GetWidth ())
#define SCREENHEIGHT (screen->GetHeight ())
#define SCREENPITCH (screen->GetPitch ())

EXTERN_CVAR (Float, vid_gamma)


// Allocates buffer screens, call before R_Init.
void V_InitScreenSize();
void V_InitScreen();

// Initializes graphics mode for the first time.
void V_Init2 ();

void V_Shutdown ();

inline bool IsRatioWidescreen(int ratio) { return (ratio & 3) != 0; }
extern bool setsizeneeded, setmodeneeded;


#endif // __V_VIDEO_H__
