#ifndef __GL_RENDERER_H
#define __GL_RENDERER_H

#include "r_defs.h"
#include "v_video.h"
#include "vectors.h"
#include "r_renderer.h"
#include "gl/data/gl_matrix.h"

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
class FBloomExtractShader;
class FBloomCombineShader;
class FBlurShader;
class FTonemapShader;
class FLensShader;
class FPresentShader;
class F2DDrawer;

inline float DEG2RAD(float deg)
{
	return deg * float(M_PI / 180.0);
}

inline float RAD2DEG(float deg)
{
	return deg * float(180. / M_PI);
}

enum SectorRenderFlags
{
	// This is used to avoid creating too many drawinfos
	SSRF_RENDERFLOOR = 1,
	SSRF_RENDERCEILING = 2,
	SSRF_RENDER3DPLANES = 4,
	SSRF_RENDERALL = 7,
	SSRF_PROCESSED = 8,
	SSRF_SEEN = 16,
};

struct GL_IRECT
{
	int left,top;
	int width,height;


	void Offset(int xofs,int yofs)
	{
		left+=xofs;
		top+=yofs;
	}
};

enum
{
	DM_MAINVIEW,
	DM_OFFSCREEN,
	DM_PORTAL
};

class FGLRenderer
{
public:

	OpenGLFrameBuffer *framebuffer;
	GLPortal *mClipPortal;
	GLPortal *mCurrentPortal;
	int mMirrorCount;
	int mPlaneMirrorCount;
	int mLightCount;
	float mCurrentFoV;
	AActor *mViewActor;
	FShaderManager *mShaderManager;
	FSamplerManager *mSamplerManager;
	int gl_spriteindex;
	unsigned int mFBID;
	unsigned int mVAOID;
	int mOldFBID;

	FGLRenderBuffers *mBuffers;
	FBloomExtractShader *mBloomExtractShader;
	FBloomCombineShader *mBloomCombineShader;
	FBlurShader *mBlurShader;
	FTonemapShader *mTonemapShader;
	FLensShader *mLensShader;
	FPresentShader *mPresentShader;

	FTexture *gllight;
	FTexture *glpart2;
	FTexture *glpart;
	FTexture *mirrortexture;
	
	float mSky1Pos, mSky2Pos;

	FRotator mAngles;
	FVector2 mViewVector;

	FFlatVertexBuffer *mVBO;
	FSkyVertexBuffer *mSkyVBO;
	FLightBuffer *mLights;
	F2DDrawer *m2DDrawer;

	GL_IRECT mScreenViewport;
	GL_IRECT mSceneViewport;
	GL_IRECT mOutputLetterbox;
	bool mDrawingScene2D = false;
	float mCameraExposure = 1.0f;

	FGLRenderer(OpenGLFrameBuffer *fb);
	~FGLRenderer() ;

	void SetOutputViewport(GL_IRECT *bounds);
	int ScreenToWindowX(int x);
	int ScreenToWindowY(int y);

	angle_t FrustumAngle();
	void SetViewArea();
	void Set3DViewport(bool mainview);
	void Reset3DViewport();
	sector_t *RenderViewpoint (AActor * camera, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen);
	void RenderView(player_t *player);
	void SetViewAngle(DAngle viewangle);
	void SetupView(float viewx, float viewy, float viewz, DAngle viewangle, bool mirror, bool planemirror);

	void Initialize(int width, int height);

	void CreateScene();
	void RenderMultipassStuff();
	void RenderScene(int recursion);
	void RenderTranslucent();
	void DrawScene(int drawmode);
	void DrawBlend(sector_t * viewsector);

	void DrawPSprite (player_t * player,DPSprite *psp,float sx, float sy, bool hudModelStep, int OverrideShader, bool alphatexture);
	void DrawPlayerSprites(sector_t * viewsector, bool hudModelStep);
	void DrawTargeterSprites();

	void Begin2D();
	void ClearBorders();

	void ProcessLowerMiniseg(seg_t *seg, sector_t * frontsector, sector_t * backsector);
	void ProcessSprite(AActor *thing, sector_t *sector, bool thruportal);
	void ProcessParticle(particle_t *part, sector_t *sector);
	void ProcessSector(sector_t *fakesector);
	void FlushTextures();
	unsigned char *GetTextureBuffer(FTexture *tex, int &w, int &h);
	void SetupLevel();

	void RenderScreenQuad();
	void SetFixedColormap (player_t *player);
	void WriteSavePic (player_t *player, FILE *file, int width, int height);
	void EndDrawScene(sector_t * viewsector);
	void BloomScene();
	void TonemapScene();
	void LensDistortScene();
	void CopyToBackbuffer(const GL_IRECT *bounds, bool applyGamma);
	void Flush() { CopyToBackbuffer(nullptr, true); }

	void SetProjection(float fov, float ratio, float fovratio);
	void SetProjection(VSMatrix matrix); // raw matrix input from stereo 3d modes
	void SetViewMatrix(float vx, float vy, float vz, bool mirror, bool planemirror);
	void ProcessScene(bool toscreen = false);

	bool StartOffscreen();
	void EndOffscreen();

	void StartSimplePolys();
	void FinishSimplePolys();

	void FillSimplePoly(FTexture *texture, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, FDynamicColormap *colormap, int lightlevel);
};

// Global functions. Make them members of GLRenderer later?
void gl_RenderBSPNode (void *node);
bool gl_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector);
void gl_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector);

typedef enum
{
        area_normal,
        area_below,
        area_above,
		area_default
} area_t;

extern area_t			in_area;


sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, area_t in_area, bool back);
inline sector_t * gl_FakeFlat(sector_t * sec, sector_t * dest, bool back)
{
	return gl_FakeFlat(sec, dest, in_area, back);
}

struct TexFilter_s
{
	int minfilter;
	int magfilter;
	bool mipmapping;
} ;


extern FGLRenderer *GLRenderer;

#endif