#pragma once

#include "v_palette.h"
#include "r_data/matrix.h"

class FMaterial;
class FRenderState;
class IVertexBuffer;

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

struct HWSkyPortal;
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
	void SetupMatrices(FMaterial *tex, float x_offset, float y_offset, bool mirror, int mode, VSMatrix &modelmatrix, VSMatrix &textureMatrix);
	std::pair<IVertexBuffer *, IIndexBuffer *> GetBufferObjects() const
	{
		return std::make_pair(mVertexBuffer, nullptr);
	}

	int FaceStart(int i)
	{
		if (i >= 0 && i < 7) return mFaceStart[i];
		else return mSideStart;
	}

};
