#pragma once
#include "model.h"

#define MD3_MAGIC			0x33504449

class FMD3Model : public FModel
{
	struct MD3Tag
	{
		// Currently I have no use for this
	};

	struct MD3TexCoord
	{
		float s,t;
	};

	struct MD3Vertex
	{
		float x,y,z;
		float nx,ny,nz;
	};

	struct MD3Triangle
	{
		int VertIndex[3];
	};

	struct MD3Surface
	{
		unsigned numVertices;
		unsigned numTriangles;
		unsigned numSkins;

		TArray<FTextureID> Skins;
		TArray<MD3Triangle> Tris;
		TArray<MD3TexCoord> Texcoords;
		TArray<MD3Vertex> Vertices;

		unsigned int vindex = UINT_MAX;	// contains numframes arrays of vertices
		unsigned int iindex = UINT_MAX;

		void UnloadGeometry()
		{
			Tris.Reset();
			Vertices.Reset();
			Texcoords.Reset();
		}
	};

	struct MD3Frame
	{
		// The bounding box information is of no use in the Doom engine
		// That will still be done with the actor's size information.
		char Name[16];
		float origin[3];
	};

	int numTags;
	int mLumpNum;

	TArray<MD3Frame> Frames;
	TArray<MD3Surface> Surfaces;

public:
	FMD3Model() = default;

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length) override;
	virtual int FindFrame(const char* name, bool nodefault) override;
	virtual void RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frame, int frame2, double inter, FTranslationID translation, const FTextureID* surfaceskinids, const TArray<VSMatrix>& boneData, int boneStartPosition) override;
	void LoadGeometry();
	void BuildVertexBuffer(FModelRenderer *renderer);
	virtual void AddSkins(uint8_t *hitlist, const FTextureID* surfaceskinids) override;
};

