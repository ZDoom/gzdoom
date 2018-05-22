#ifndef __GL_RENDERER_H
#define __GL_RENDERER_H

#include "r_defs.h"
#include "v_video.h"
#include "vectors.h"
#include "r_renderer.h"
#include "r_data/matrix.h"
#include "gl/dynlights/gl_shadowmap.h"
#include <functional>

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

struct particle_t;
class FCanvasTexture;
class FFlatVertexBuffer;
class FSkyVertexBuffer;
class OpenGLFrameBuffer;
struct FDrawInfo;
class FShaderManager;
class GLPortal;
class FLightBuffer;
class FSamplerManager;
class DPSprite;
class FGLRenderBuffers;
class FLinearDepthShader;
class FDepthBlurShader;
class FSSAOShader;
class FSSAOCombineShader;
class FBloomExtractShader;
class FBloomCombineShader;
class FExposureExtractShader;
class FExposureAverageShader;
class FExposureCombineShader;
class FBlurShader;
class FTonemapShader;
class FColormapShader;
class FLensShader;
class FFXAALumaShader;
class FFXAAShader;
class FPresentShader;
class FPresent3DCheckerShader;
class FPresent3DColumnShader; 
class FPresent3DRowShader;
class FGL2DDrawer;
class FHardwareTexture;
class FShadowMapShader;
class FCustomPostProcessShaders;
class GLSceneDrawer;
class SWSceneDrawer;

enum
{
	DM_MAINVIEW,
	DM_OFFSCREEN,
	DM_PORTAL,
	DM_SKYPORTAL
};


// Helper baggage to draw the paletted software renderer output on old hardware.
// This must be here because the 2D drawer needs to access it, not the scene drawer.
class LegacyShader;
struct LegacyShaderContainer
{
	enum
	{
		NUM_SHADERS = 4
	};

	LegacyShader *Shaders[NUM_SHADERS];

	LegacyShader* CreatePixelShader(const FString& vertexsrc, const FString& fragmentsrc, const FString &defines);
	LegacyShaderContainer();
	~LegacyShaderContainer();
	bool LoadShaders();
	void BindShader(int num, const float *p1, const float *p2);
};




class FGLRenderer
{
public:

	OpenGLFrameBuffer *framebuffer;
	//GLPortal *mClipPortal;
	GLPortal *mCurrentPortal;
	int mMirrorCount;
	int mPlaneMirrorCount;
	float mCurrentFoV;
	AActor *mViewActor;
	FShaderManager *mShaderManager;
	FSamplerManager *mSamplerManager;
	unsigned int mFBID;
	unsigned int mVAOID;
	int mOldFBID;

	FGLRenderBuffers *mBuffers;
	FLinearDepthShader *mLinearDepthShader;
	FSSAOShader *mSSAOShader;
	FDepthBlurShader *mDepthBlurShader;
	FSSAOCombineShader *mSSAOCombineShader;
	FBloomExtractShader *mBloomExtractShader;
	FBloomCombineShader *mBloomCombineShader;
	FExposureExtractShader *mExposureExtractShader;
	FExposureAverageShader *mExposureAverageShader;
	FExposureCombineShader *mExposureCombineShader;
	FBlurShader *mBlurShader;
	FTonemapShader *mTonemapShader;
	FColormapShader *mColormapShader;
	FHardwareTexture *mTonemapPalette;
	FLensShader *mLensShader;
	FFXAALumaShader *mFXAALumaShader;
	FFXAAShader *mFXAAShader;
	FPresentShader *mPresentShader;
	FPresent3DCheckerShader *mPresent3dCheckerShader;
	FPresent3DColumnShader *mPresent3dColumnShader;
	FPresent3DRowShader *mPresent3dRowShader;
	FShadowMapShader *mShadowMapShader;
	FCustomPostProcessShaders *mCustomPostProcessShaders;

	FShadowMap mShadowMap;

	FRotator mAngles;
	FVector2 mViewVector;

	FFlatVertexBuffer *mVBO;
	FSkyVertexBuffer *mSkyVBO;
	FLightBuffer *mLights;
	SWSceneDrawer *swdrawer = nullptr;
	LegacyShaderContainer *legacyShaders = nullptr;

	bool mDrawingScene2D = false;
	bool buffersActive = false;

	float mSceneClearColor[3];

	float mGlobVis = 0.0f;

	FGLRenderer(OpenGLFrameBuffer *fb);
	~FGLRenderer() ;

	void Initialize(int width, int height);

	void ClearBorders();

	void FlushTextures();
	void SetupLevel();
	void ResetSWScene();

	void RenderScreenQuad();
	void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D);
	void AmbientOccludeScene();
	void UpdateCameraExposure();
	void BloomScene(int fixedcm);
	void TonemapScene();
	void ColormapScene(int fixedcm);
	void CreateTonemapPalette();
	void ClearTonemapPalette();
	void LensDistortScene();
	void ApplyFXAA();
	void BlurScene(float gameinfobluramount);
	void CopyToBackbuffer(const IntRect *bounds, bool applyGamma);
	void DrawPresentTexture(const IntRect &box, bool applyGamma);
	void Flush();
	void Draw2D(F2DDrawer *data);
	void RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV);
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height);
	sector_t *RenderView(player_t *player);
	void BeginFrame();

	bool StartOffscreen();
	void EndOffscreen();

	void FillSimplePoly(FTexture *texture, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip);

	static float GetZNear() { return 5.f; }
	static float GetZFar() { return 65536.f; }
};

#include "hwrenderer/scene/hw_fakeflat.h"

struct TexFilter_s
{
	int minfilter;
	int magfilter;
	bool mipmapping;
} ;


extern FGLRenderer *GLRenderer;

#endif
