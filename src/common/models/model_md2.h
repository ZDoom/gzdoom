#pragma once
#include "model.h"

#define MD2_MAGIC			0x32504449
#define DMD_MAGIC			0x4D444D44
#define MAX_LODS 4

class FDMDModel : public FModel
{
protected:

	struct FTriangle
	{
		unsigned short           vertexIndices[3];
		unsigned short           textureIndices[3];
	};


	struct DMDHeader
	{
		int             magic;
		int             version;
		int             flags;
	};

	struct DMDModelVertex
	{
		float           xyz[3];
	};

	struct FTexCoord
	{
		short           s, t;
	};

	struct FGLCommandVertex
	{
		float           s, t;
		int             index;
	};

	struct DMDInfo
	{
		int             skinWidth;
		int             skinHeight;
		int             frameSize;
		int             numSkins;
		int             numVertices;
		int             numTexCoords;
		int             numFrames;
		int             numLODs;
		int             offsetSkins;
		int             offsetTexCoords;
		int             offsetFrames;
		int             offsetLODs;
		int             offsetEnd;
	};

	struct ModelFrame
	{
		char            name[16];
		unsigned int vindex;
	};

	struct ModelFrameVertexData
	{
		DMDModelVertex *vertices;
		DMDModelVertex *normals;
	};

	struct DMDLoDInfo
	{
		int             numTriangles;
		int             numGlCommands;
		int             offsetTriangles;
		int             offsetGlCommands;
	};

	struct DMDLoD
	{
		FTriangle		* triangles;
	};


	int				mLumpNum;
	DMDHeader	    header;
	DMDInfo			info;
	FTextureID *	skins;
	ModelFrame  *	frames;
	bool			allowTexComp;  // Allow texture compression with this.

	// Temp data only needed for buffer construction
	FTexCoord *		texCoords;
	ModelFrameVertexData *framevtx;
	DMDLoDInfo		lodInfo[MAX_LODS];
	DMDLoD			lods[MAX_LODS];

public:
	FDMDModel() 
	{ 
		mLumpNum = -1;
		frames = NULL;
		skins = NULL;
		for (int i = 0; i < MAX_LODS; i++)
		{
			lods[i].triangles = NULL;
		}
		info.numLODs = 0;
		texCoords = NULL;
		framevtx = NULL;
	}
	virtual ~FDMDModel();

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length) override;
	virtual int FindFrame(const char* name, bool nodefault) override;
	virtual void RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frame, int frame2, double inter, FTranslationID translation, const FTextureID* surfaceskinids, const TArray<VSMatrix>& boneData, int boneStartPosition) override;
	virtual void LoadGeometry();
	virtual void AddSkins(uint8_t *hitlist, const FTextureID* surfaceskinids) override;

	void UnloadGeometry();
	void BuildVertexBuffer(FModelRenderer *renderer);

};

// This uses the same internal representation as DMD
class FMD2Model : public FDMDModel
{
public:
	FMD2Model() {}
	virtual ~FMD2Model();

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length);
	virtual void LoadGeometry();

};


