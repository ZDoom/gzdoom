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
	TArray<unsigned int> mPrimStart;

	int mRows, mColumns;

	// indices for sky cubemap faces
	int mFaceStart[7];
	int mSideStart;

	void SkyVertex(int r, int c, bool yflip);
	void CreateSkyHemisphere(int hemi);
	void CreateDome();

public:

	FSkyVertexBuffer();
	~FSkyVertexBuffer();
	void SetupMatrices(FGameTexture *tex, float x_offset, float y_offset, bool mirror, int mode, VSMatrix &modelmatrix, VSMatrix &textureMatrix, bool tiled);
	std::pair<IVertexBuffer *, IIndexBuffer *> GetBufferObjects() const
	{
		return std::make_pair(mVertexBuffer, nullptr);
	}

	int FaceStart(int i)
	{
		if (i >= 0 && i < 7) return mFaceStart[i];
		else return mSideStart;
	}

	void RenderRow(FRenderState& state, EDrawType prim, int row, bool apply = true);
	void RenderDome(FRenderState& state, FGameTexture* tex, float x_offset, float y_offset, bool mirror, int mode, bool tiled);
	void RenderBox(FRenderState& state, FTextureID texno, FSkyBox* tex, float x_offset, bool sky2, float stretch, const FVector3& skyrotatevector, const FVector3& skyrotatevector2);

};
