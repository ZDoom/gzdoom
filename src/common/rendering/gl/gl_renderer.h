#ifndef __GL_RENDERER_H
#define __GL_RENDERER_H

#include "v_video.h"
#include "vectors.h"
#include "matrix.h"
#include "gl_renderbuffers.h"
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

namespace OpenGLRenderer
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
	FSamplerManager *mSamplerManager = nullptr;
	unsigned int mFBID;
	unsigned int mVAOID;
	unsigned int mStencilValue = 0;

	int mOldFBID;

	FGLRenderBuffers *mBuffers = nullptr;
	FGLRenderBuffers *mScreenBuffers = nullptr;
	FGLRenderBuffers *mSaveBuffers = nullptr;
	FPresentShader *mPresentShader = nullptr;
	FPresent3DCheckerShader *mPresent3dCheckerShader = nullptr;
	FPresent3DColumnShader *mPresent3dColumnShader = nullptr;
	FPresent3DRowShader *mPresent3dRowShader = nullptr;
	FShadowMapShader *mShadowMapShader = nullptr;

	int mLightMapID = 0;

	//FRotator mAngles;

	FGLRenderer(OpenGLFrameBuffer *fb);
	~FGLRenderer() ;

	void Initialize(int width, int height);

	void ClearBorders();

	void PresentStereo();
	void RenderScreenQuad();
	void PostProcessScene(int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D);
	void AmbientOccludeScene(float m5);
	void ClearTonemapPalette();
	void BlurScene(float gameinfobluramount);
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
