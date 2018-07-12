#pragma once

#include "v_palette.h"

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

class FSkyDomeCreator
{
public:
	static const int SKYHEMI_UPPER = 1;
	static const int SKYHEMI_LOWER = 2;

	enum
	{
		SKYMODE_MAINLAYER = 0,
		SKYMODE_SECONDLAYER = 1,
		SKYMODE_FOGLAYER = 2
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
	void RenderDome(FMaterial *tex, int mode);

	int FaceStart(int i)
	{
		if (i >= 0 && i < 7) return mFaceStart[i];
		else return mSideStart;
	}

};
