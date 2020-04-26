// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#ifndef __GL_MODELS_H_
#define __GL_MODELS_H_

#include "tarray.h"
#include "matrix.h"
#include "m_bbox.h"
#include "r_defs.h"
#include "g_levellocals.h"
#include "r_data/voxels.h"
#include "i_modelvertexbuffer.h"

class FModelRenderer;

#define MAX_LODS			4

enum { VX, VZ, VY };

#define MD2_MAGIC			0x32504449
#define DMD_MAGIC			0x4D444D44
#define MD3_MAGIC			0x33504449
#define NUMVERTEXNORMALS	162
#define MD3_MAX_SURFACES	32

FTextureID LoadSkin(const char * path, const char * fn);

struct FSpriteModelFrame;
class IModelVertexBuffer;
struct FLevelLocals;
class FModel;

enum ModelRendererType
{
	GLModelRendererType,
	SWModelRendererType,
	PolyModelRendererType,
	NumModelRendererTypes
};

class FModel
{
public:
	FModel();
	virtual ~FModel();

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length) = 0;
	virtual int FindFrame(const char * name) = 0;
	virtual void RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frame, int frame2, double inter, int translation=0) = 0;
	virtual void BuildVertexBuffer(FModelRenderer *renderer) = 0;
	virtual void AddSkins(uint8_t *hitlist) = 0;
	virtual float getAspectFactor(float vscale) { return 1.f; }

	void SetVertexBuffer(int type, IModelVertexBuffer *buffer) { mVBuf[type] = buffer; }
	IModelVertexBuffer *GetVertexBuffer(int type) const { return mVBuf[type]; }
	void DestroyVertexBuffer();

	const FSpriteModelFrame *curSpriteMDLFrame;
	int curMDLIndex;
	void PushSpriteMDLFrame(const FSpriteModelFrame *smf, int index) { curSpriteMDLFrame = smf; curMDLIndex = index; };

	FString mFileName;

private:
	IModelVertexBuffer *mVBuf[NumModelRendererTypes];
};

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

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length);
	virtual int FindFrame(const char * name);
	virtual void RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frame, int frame2, double inter, int translation=0);
	virtual void LoadGeometry();
	virtual void AddSkins(uint8_t *hitlist);

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

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length);
	virtual int FindFrame(const char * name);
	virtual void RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frame, int frame2, double inter, int translation=0);
	void LoadGeometry();
	void BuildVertexBuffer(FModelRenderer *renderer);
	virtual void AddSkins(uint8_t *hitlist);
};

struct FVoxelVertexHash
{
	// Returns the hash value for a key.
	hash_t Hash(const FModelVertex &key) 
	{ 
		int ix = xs_RoundToInt(key.x);		
		int iy = xs_RoundToInt(key.y);		
		int iz = xs_RoundToInt(key.z);		
		return (hash_t)(ix + (iy<<9) + (iz<<18));
	}

	// Compares two keys, returning zero if they are the same.
	int Compare(const FModelVertex &left, const FModelVertex &right) 
	{ 
		return left.x != right.x || left.y != right.y || left.z != right.z || left.u != right.u || left.v != right.v;
	}
};

struct FIndexInit
{
	void Init(unsigned int &value)
	{
		value = 0xffffffff;
	}
};

typedef TMap<FModelVertex, unsigned int, FVoxelVertexHash, FIndexInit> FVoxelMap;


class FVoxelModel : public FModel
{
protected:
	FVoxel *mVoxel;
	bool mOwningVoxel;	// if created through MODELDEF deleting this object must also delete the voxel object
	FTextureID mPalette;
	unsigned int mNumIndices;
	TArray<FModelVertex> mVertices;
	TArray<unsigned int> mIndices;
	
	void MakeSlabPolys(int x, int y, kvxslab_t *voxptr, FVoxelMap &check);
	void AddFace(int x1, int y1, int z1, int x2, int y2, int z2, int x3, int y3, int z3, int x4, int y4, int z4, uint8_t color, FVoxelMap &check);
	unsigned int AddVertex(FModelVertex &vert, FVoxelMap &check);

public:
	FVoxelModel(FVoxel *voxel, bool owned);
	~FVoxelModel();
	bool Load(const char * fn, int lumpnum, const char * buffer, int length);
	void Initialize();
	virtual int FindFrame(const char * name);
	virtual void RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frame, int frame2, double inter, int translation=0);
	virtual void AddSkins(uint8_t *hitlist);
	FTextureID GetPaletteTexture() const { return mPalette; }
	void BuildVertexBuffer(FModelRenderer *renderer);
	float getAspectFactor(float vscale) override;
};



#define MAX_MODELS_PER_FRAME 4

//
// [BB] Model rendering flags.
//
enum
{
	// [BB] Color translations for the model skin are ignored. This is
	// useful if the skin texture is not using the game palette.
	MDL_IGNORETRANSLATION			= 1,
	MDL_PITCHFROMMOMENTUM			= 2,
	MDL_ROTATING					= 4,
	MDL_INTERPOLATEDOUBLEDFRAMES	= 8,
	MDL_NOINTERPOLATION				= 16,
	MDL_USEACTORPITCH				= 32,
	MDL_USEACTORROLL				= 64,
	MDL_BADROTATION					= 128,
	MDL_DONTCULLBACKFACES			= 256,
	MDL_USEROTATIONCENTER			= 512,
};

struct FSpriteModelFrame
{
	int modelIDs[MAX_MODELS_PER_FRAME];
	FTextureID skinIDs[MAX_MODELS_PER_FRAME];
	FTextureID surfaceskinIDs[MAX_MODELS_PER_FRAME][MD3_MAX_SURFACES];
	int modelframes[MAX_MODELS_PER_FRAME];
	float xscale, yscale, zscale;
	// [BB] Added zoffset, rotation parameters and flags.
	// Added xoffset, yoffset
	float xoffset, yoffset, zoffset;
	float xrotate, yrotate, zrotate;
	float rotationCenterX, rotationCenterY, rotationCenterZ;
	float rotationSpeed;
	unsigned int flags;
	const PClass * type;
	short sprite;
	short frame;
	FState * state;	// for later!
	int hashnext;
	float angleoffset;
	// added pithoffset, rolloffset.
	float pitchoffset, rolloffset; // I don't want to bother with type transformations, so I made this variables float.
	bool isVoxel;
};

FSpriteModelFrame * FindModelFrame(const PClass * ti, int sprite, int frame, bool dropped);
bool IsHUDModelForPlayerAvailable(player_t * player);
void FlushModels();


extern TDeletingArray<FModel*> Models;

// Check if circle potentially intersects with node AABB
inline bool CheckBBoxCircle(float *bbox, float x, float y, float radiusSquared)
{
	float centerX = (bbox[BOXRIGHT] + bbox[BOXLEFT]) * 0.5f;
	float centerY = (bbox[BOXBOTTOM] + bbox[BOXTOP]) * 0.5f;
	float extentX = (bbox[BOXRIGHT] - bbox[BOXLEFT]) * 0.5f;
	float extentY = (bbox[BOXBOTTOM] - bbox[BOXTOP]) * 0.5f;
	float aabbRadiusSquared = extentX * extentX + extentY * extentY;
	x -= centerX;
	y -= centerY;
	float dist = x * x + y * y;
	return dist <= radiusSquared + aabbRadiusSquared;
}

// Helper function for BSPWalkCircle
template<typename Callback>
void BSPNodeWalkCircle(void *node, float x, float y, float radiusSquared, const Callback &callback)
{
	while (!((size_t)node & 1))
	{
		node_t *bsp = (node_t *)node;

		if (CheckBBoxCircle(bsp->bbox[0], x, y, radiusSquared))
			BSPNodeWalkCircle(bsp->children[0], x, y, radiusSquared, callback);

		if (!CheckBBoxCircle(bsp->bbox[1], x, y, radiusSquared))
			return;

		node = bsp->children[1];
	}

	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
	callback(sub);
}

// Search BSP for subsectors within the given radius and call callback(subsector) for each found
template<typename Callback>
void BSPWalkCircle(FLevelLocals *Level, float x, float y, float radiusSquared, const Callback &callback)
{
	if (Level->nodes.Size() == 0)
		callback(&Level->subsectors[0]);
	else
		BSPNodeWalkCircle(Level->HeadNode(), x, y, radiusSquared, callback);
}

void RenderModel(FModelRenderer* renderer, float x, float y, float z, FSpriteModelFrame* smf, AActor* actor, double ticFrac);
void RenderHUDModel(FModelRenderer* renderer, DPSprite* psp, float ofsX, float ofsY);

#endif
