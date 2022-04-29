#ifndef __GL_RENDERER_H
#define __GL_RENDERER_H

#include "v_video.h"
#include "vectors.h"
#include "matrix.h"
#include "gles_renderbuffers.h"
#include <functional>

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

struct particle_t;
class FCanvasTexture;
class FFlatVertexBuffer;
class FSkyVertexBuffer;
class HWPortal;
class FLightBuffer;
class DPSprite;
class FGLRenderBuffers;
class FGL2DDrawer;
class SWSceneDrawer;
class HWViewpointBuffer;
struct FRenderViewpoint;

namespace OpenGLESRenderer
{
	class FHardwareTexture;
	class FShaderManager;
	class FSamplerManager;
	class OpenGLFrameBuffer;
	class FPresentShaderBase;
	class FPresentShader;
	class FPresent3DCheckerShader;
	class FPresent3DColumnShader;
	class FPresent3DRowShader;
	class FShadowMapShader;

class FGLRenderer
{
public:

	OpenGLFrameBuffer *framebuffer;
	int mMirrorCount = 0;
	int mPlaneMirrorCount = 0;
	FShaderManager *mShaderManager = nullptr;
	FSamplerManager* mSamplerManager = nullptr;
	unsigned int mFBID = 0;
	unsigned int mStencilValue = 0;

	int mOldFBID = 0;

	FGLRenderBuffers *mBuffers = nullptr;
	FGLRenderBuffers *mScreenBuffers = nullptr;
	FPresentShader *mPresentShader = nullptr;

	//FRotator mAngles;

	FGLRenderer(OpenGLFrameBuffer *fb);
	~FGLRenderer() ;

	void Initialize(int width, int height);

	void ClearBorders();

	void PresentStereo();
	void RenderScreenQuad();
	void PostProcessScene(int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D);

	void CopyToBackbuffer(const IntRect *bounds, bool applyGamma);
	void DrawPresentTexture(const IntRect &box, bool applyGamma);
	void Flush();
	void BeginFrame();

	bool StartOffscreen();
	void EndOffscreen();

	void BindToFrameBuffer(FTexture* tex);

private:

	bool QuadStereoCheckInitialRenderContextState();
	void PresentAnaglyph(bool r, bool g, bool b);
	void PresentSideBySide(int);
	void PresentTopBottom();
	void prepareInterleavedPresent(FPresentShaderBase& shader);
	void PresentColumnInterleaved();
	void PresentRowInterleaved();
	void PresentCheckerInterleaved();
	void PresentQuadStereo();

};

struct TexFilter_s
{
	int minfilter;
	int magfilter;
	bool mipmapping;
} ;


extern FGLRenderer *GLRenderer;

}
#endif
