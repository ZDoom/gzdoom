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
#include "r_data/matrix.h"
#include "actor.h"
#include "dobject.h"
#include "p_pspr.h"
#include "r_data/voxels.h"
#include "info.h"
#include "g_levellocals.h"

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

enum ModelRendererType
{
	GLModelRendererType,
	SWModelRendererType,
	PolyModelRendererType,
	NumModelRendererTypes
};

class FModelRenderer
{
public:
	virtual ~FModelRenderer() { }

	void RenderModel(float x, float y, float z, FSpriteModelFrame *modelframe, AActor *actor);
	void RenderHUDModel(DPSprite *psp, float ofsx, float ofsy);

	virtual ModelRendererType GetType() const = 0;

	virtual void BeginDrawModel(AActor *actor, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix, bool mirrored) = 0;
	virtual void EndDrawModel(AActor *actor, FSpriteModelFrame *smf) = 0;

	virtual IModelVertexBuffer *CreateVertexBuffer(bool needindex, bool singleframe) = 0;

	virtual void SetVertexBuffer(IModelVertexBuffer *buffer) = 0;
	virtual void ResetVertexBuffer() = 0;

	virtual VSMatrix GetViewToWorldMatrix() = 0;

	virtual void BeginDrawHUDModel(AActor *actor, const VSMatrix &objectToWorldMatrix, bool mirrored) = 0;
	virtual void EndDrawHUDModel(AActor *actor) = 0;

	virtual void SetInterpolation(double interpolation) = 0;
	virtual void SetMaterial(FTexture *skin, bool clampNoFilter, int translation) = 0;
	virtual void DrawArrays(int start, int count) = 0;
	virtual void DrawElements(int numIndices, size_t offset) = 0;

private:
	void RenderFrameModels(const FSpriteModelFrame *smf, const FState *curState, const int curTics, const PClass *ti, int translation);
};

struct FModelVertex
{
	float x, y, z;	// world position
	float u, v;		// texture coordinates
	unsigned packedNormal;	// normal vector as GL_INT_2_10_10_10_REV.

	void Set(float xx, float yy, float zz, float uu, float vv)
	{
		x = xx;
		y = yy;
		z = zz;
		u = uu;
		v = vv;
	}

	void SetNormal(float nx, float ny, float nz)
	{
		int inx = clamp(int(nx * 512), -512, 511);
		int iny = clamp(int(ny * 512), -512, 511);
		int inz = clamp(int(nz * 512), -512, 511);
		int inw = 0;
		packedNormal = (inw << 30) | ((inz & 1023) << 20) | ((iny & 1023) << 10) | (inx & 1023);
	}
};

#define VMO ((FModelVertex*)NULL)

class FModelRenderer;

class IModelVertexBuffer
{
public:
	virtual ~IModelVertexBuffer() { }

	virtual FModelVertex *LockVertexBuffer(unsigned int size) = 0;
	virtual void UnlockVertexBuffer() = 0;

	virtual unsigned int *LockIndexBuffer(unsigned int size) = 0;
	virtual void UnlockIndexBuffer() = 0;

	virtual void SetupFrame(FModelRenderer *renderer, unsigned int frame1, unsigned int frame2, unsigned int size) = 0;
};

class FModel
{
public:
	FModel();
	virtual ~FModel();

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length) = 0;
	virtual int FindFrame(const char * name) = 0;
	virtual void RenderFrame(FModelRenderer *renderer, FTexture * skin, int frame, int frame2, double inter, int translation=0) = 0;
	virtual void BuildVertexBuffer(FModelRenderer *renderer) = 0;
	virtual void AddSkins(uint8_t *hitlist) = 0;
	virtual float getAspectFactor() { return 1.f; }

	void SetVertexBuffer(FModelRenderer *renderer, IModelVertexBuffer *buffer) { mVBuf[renderer->GetType()] = buffer; }
	IModelVertexBuffer *GetVertexBuffer(FModelRenderer *renderer) const { return mVBuf[renderer->GetType()]; }
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
	virtual void RenderFrame(FModelRenderer *renderer, FTexture * skin, int frame, int frame2, double inter, int translation=0);
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
		int numVertices;
		int numTriangles;
		int numSkins;

		FTextureID * skins;
		MD3Triangle * tris;
		MD3TexCoord * texcoords;
		MD3Vertex * vertices;

		unsigned int vindex;	// contains numframes arrays of vertices
		unsigned int iindex;

		MD3Surface()
		{
			tris=NULL;
			vertices=NULL;
			texcoords=NULL;
			vindex = iindex = UINT_MAX;
		}

		~MD3Surface()
		{
			if (skins) delete [] skins;
			UnloadGeometry();
		}

		void UnloadGeometry()
		{
			if (tris) delete [] tris;
			if (vertices) delete [] vertices;
			if (texcoords) delete [] texcoords;
			tris = NULL;
			vertices = NULL;
			texcoords = NULL;
		}
	};

	struct MD3Frame
	{
		// The bounding box information is of no use in the Doom engine
		// That will still be done with the actor's size information.
		char Name[16];
		float origin[3];
	};

	int numFrames;
	int numTags;
	int numSurfaces;
	int mLumpNum;

	MD3Frame * frames;
	MD3Surface * surfaces;

public:
	FMD3Model() { }
	virtual ~FMD3Model();

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length);
	virtual int FindFrame(const char * name);
	virtual void RenderFrame(FModelRenderer *renderer, FTexture * skin, int frame, int frame2, double inter, int translation=0);
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
	virtual void RenderFrame(FModelRenderer *renderer, FTexture * skin, int frame, int frame2, double inter, int translation=0);
	virtual void AddSkins(uint8_t *hitlist);
	FTextureID GetPaletteTexture() const { return mPalette; }
	void BuildVertexBuffer(FModelRenderer *renderer);
	float getAspectFactor();
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
};

FSpriteModelFrame * FindModelFrame(const PClass * ti, int sprite, int frame, bool dropped);
bool IsHUDModelForPlayerAvailable(player_t * player);
void FlushModels();

class DeletingModelArray : public TArray<FModel *>
{
public:

	~DeletingModelArray()
	{
		for (unsigned i = 0; i<Size(); i++)
		{
			delete (*this)[i];
		}

	}
};

extern DeletingModelArray Models;

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
void BSPWalkCircle(float x, float y, float radiusSquared, const Callback &callback)
{
	if (level.nodes.Size() == 0)
		callback(&level.subsectors[0]);
	else
		BSPNodeWalkCircle(level.HeadNode(), x, y, radiusSquared, callback);
}

#endif
