#pragma once

#include "models.h"

class FUE1Model : public FModel
{
public:
	bool Load(const char * fn, int lumpnum, const char * buffer, int length) override;
	int FindFrame(const char * name) override;
	void RenderFrame(FModelRenderer *renderer, FTexture * skin, int frame, int frame2, double inter, int translation=0) override;
	void BuildVertexBuffer(FModelRenderer *renderer) override;
	void AddSkins(uint8_t *hitlist) override;
	void LoadGeometry();
	void UnloadGeometry();
	FUE1Model()
	{
		mLumpNum = -1;
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
	int mLumpNum;

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

	// converted data structures
	struct UE1Vertex
	{
		FVector3 Pos, Normal;
	};
	struct UE1Poly
	{
		int V[3];
		FVector2 C[3];
		int texNum;
	};
	struct UE1Group
	{
		TArray<int> P;
		int numPolys;
	};

	int numVerts;
	int numFrames;
	int numPolys;
	int numGroups;

	TArray<UE1Vertex> verts;
	TArray<UE1Poly> polys;
	TArray<UE1Group> groups;
	TArray<int> groupIndices;
};
