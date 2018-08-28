#pragma once

#include "v_palette.h"
#include "r_data/matrix.h"

class FMaterial;
struct AttributeBufferData;
struct GLSkyInfo;
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

struct FSkyBufferInfo
{
	int mModelMatrixIndex;
	int mAttributeIndex;
	int mFlags;
};

class FSkyDomeCreator
{
public:
	static const int SKYHEMI_UPPER = 1;
	static const int SKYHEMI_LOWER = 2;

	enum
	{
		SKYMODE_MAINLAYER = 1,
		SKYMODE_SECONDLAYER = 2,
		SKYMODE_FOGLAYER = 4
	};

protected:
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
	
	FSkyDomeCreator();
	virtual ~FSkyDomeCreator();
	void SetupMatrices(FMaterial *tex, float x_offset, float y_offset, bool mirror, int mode, VSMatrix &modelmatrix, VSMatrix &textureMatrix);
	int CreateAttributesForDome(AttributeBufferData &work, HWDrawInfo *di, FMaterial *tex, int mode);
	std::tuple<int, int> PrepareDome(AttributeBufferData &work, HWDrawInfo *di, FMaterial * tex, float x_offset, float y_offset, bool mirror, int mode);
	std::tuple<int, int> PrepareBox(AttributeBufferData &work, HWDrawInfo *di,FTextureID texno, FMaterial * gltex, float x_offset, bool sky2);
	FSkyBufferInfo PrepareContents(HWDrawInfo *di, GLSkyInfo *origin);


	int FaceStart(int i)
	{
		if (i >= 0 && i < 7) return mFaceStart[i];
		else return mSideStart;
	}

};
