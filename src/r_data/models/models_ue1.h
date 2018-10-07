#pragma once

#include "models.h"

class FUE1Model : public FModel
{
public:
	enum EPolyType
	{
		PT_Normal = 0,		// normal renderstyle
		PT_TwoSided = 1,	// like normal, but don't cull backfaces
		PT_Translucent = 2,	// additive blending
		PT_Masked = 3,		// draw with alpha testing
		PT_Modulated = 4,	// overlay-like blending (rgb values below 128 darken, 128 is unchanged, and above 128 lighten)
		// types mask
		PT_Type = 7,
		// flags
		PT_WeaponTriangle = 0x08,	// this poly is used for positioning a weapon attachment and should not be drawn
		PT_Unlit = 0x10,		// this poly is fullbright
		PT_Curvy = 0x20,		// this poly uses the facet normal
		PT_EnvironmentMap = 0x40,	// vertex UVs are remapped to their view-space X and Z normals, fake cubemap look
		PT_NoSmooth = 0x80		// this poly forcibly uses nearest filtering
	};

	bool Load(const char * fn, int lumpnum, const char * buffer, int length) override;
	int FindFrame(const char * name) override;
	void RenderFrame(FModelRenderer *renderer, FTexture * skin, int frame, int frame2, double inter, int translation=0) override;
	void BuildVertexBuffer(FModelRenderer *renderer) override;
	void AddSkins(uint8_t *hitlist) override;
	void LoadGeometry();
	void UnloadGeometry();
	FUE1Model()
	{
		mDataLump = -1;
		mAnivLump = -1;
		mDataLoaded = false;
		dhead = NULL;
		dpolys = NULL;
		ahead = NULL;
		averts = NULL;
		numVerts = 0;
		numFrames = 0;
		numPolys = 0;
		numGroups = 0;
	}
	~FUE1Model();

private:
	int mDataLump, mAnivLump;
	bool mDataLoaded;

	// raw data structures
	struct d3dhead
	{
		uint16_t numpolys, numverts;
		uint16_t bogusrot, bogusframe;
		uint32_t bogusnorm[3];
		uint32_t fixscale;
		uint32_t unused[3];
		uint8_t padding[12];
	};
	struct d3dpoly
	{
		uint16_t vertices[3];
		uint8_t type, color;
		uint8_t uv[3][2];
		uint8_t texnum, flags;
	};
	struct a3dhead
	{
		uint16_t numframes, framesize;
	};
	d3dhead * dhead;
	d3dpoly * dpolys;
	a3dhead * ahead;
	uint32_t * averts;
	struct dxvert
	{
		int16_t x, y, z, pad;
	};
	dxvert * dxverts;

	// converted data structures
	struct UE1Vertex
	{
		FVector3 Pos, Normal;
	};
	struct UE1Poly
	{
		int V[3];
		FVector2 C[3];
		TArray<FVector3> Normals;
	};
	struct UE1Group
	{
		TArray<int> P;
		int numPolys, texNum, type;
	};

	int numVerts;
	int numFrames;
	int numPolys;
	int numGroups;
	int weaponPoly;	// for future model attachment support, unused for now

	TArray<UE1Vertex> verts;
	TArray<UE1Poly> polys;
	TArray<UE1Group> groups;
};
