#pragma once

#include "matrix.h"
#include "hwrenderer/data/buffers.h"
#include "hw_renderstate.h"
#include "skyboxtexture.h"

class FGameTexture;
class FRenderState;
class IVertexBuffer;
struct HWSkyPortal;
struct HWDrawInfo;

// 57 world units roughly represent one sky texel for the glTranslate call.
const int skyoffsetfactor = 57;

struct FSkyVertex
{
	float x, y, z, u, v;
	PalEntry color;

	void Set(float xx, float zz, float yy, float uu=0, float vv=0, PalEntry col=0xffffffff)
	{
		x = xx;
		z = zz;
		y = yy;
		u = uu;
		v = vv;
		color = col;
	}

	void SetXYZ(float xx, float yy, float zz, float uu = 0, float vv = 0, PalEntry col = 0xffffffff)
	{
		x = xx;
		y = yy;
		z = zz;
		u = uu;
		v = vv;
		color = col;
	}

};

class FSkyVertexBuffer
{
	friend struct HWSkyPortal;
public:
	static const int SKYHEMI_UPPER = 1;
	static const int SKYHEMI_LOWER = 2;

	enum
	{
		SKYMODE_MAINLAYER = 0,
		SKYMODE_SECONDLAYER = 1,
		SKYMODE_FOGLAYER = 2
	};

	IVertexBuffer *mVertexBuffer;

	TArray<FSkyVertex> mVertices;
	TArray<unsigned int> mPrimStartDoom;
	TArray<unsigned int> mPrimStartBuild;

	int mRows, mColumns;

	// indices for sky cubemap faces
	int mFaceStart[7];
	int mSideStart;

	void SkyVertexDoom(int r, int c, bool yflip);
	void SkyVertexBuild(int r, int c, bool yflip);
	void CreateSkyHemisphereDoom(int hemi);
	void CreateSkyHemisphereBuild(int hemi);
	void CreateDome();

public:

	FSkyVertexBuffer();
	~FSkyVertexBuffer();
	void SetupMatrices(FGameTexture *tex, float x_offset, float y_offset, bool mirror, int mode, VSMatrix &modelmatrix, VSMatrix &textureMatrix, bool tiled, float xscale = 0, float vertscale = 0);
	std::pair<IVertexBuffer *, IIndexBuffer *> GetBufferObjects() const
	{
		return std::make_pair(mVertexBuffer, nullptr);
	}

	int FaceStart(int i)
	{
		if (i >= 0 && i < 7) return mFaceStart[i];
		else return mSideStart;
	}

	void RenderRow(FRenderState& state, EDrawType prim, int row, TArray<unsigned int>& mPrimStart, bool apply = true);
	void DoRenderDome(FRenderState& state, FGameTexture* tex, int mode, bool which, PalEntry color = 0xffffffff);
	void RenderDome(FRenderState& state, FGameTexture* tex, float x_offset, float y_offset, bool mirror, int mode, bool tiled, float xscale = 0, float yscale = 0, PalEntry color = 0xffffffff);
	void RenderBox(FRenderState& state, FSkyBox* tex, float x_offset, bool sky2, float stretch, const FVector3& skyrotatevector, const FVector3& skyrotatevector2, PalEntry color = 0xffffffff);

};
