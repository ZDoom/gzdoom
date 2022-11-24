#pragma once

#include <stdint.h>
#include "textureid.h"
#include "i_modelvertexbuffer.h"
#include "matrix.h"
#include "TRS.h"

class DBoneComponents;
class FModelRenderer;
class FGameTexture;
class IModelVertexBuffer;
class FModel;
struct FSpriteModelFrame;

FTextureID LoadSkin(const char* path, const char* fn);
void FlushModels();
extern TArray<FString> savedModelFiles;
extern TDeletingArray<FModel*> Models;
extern TArray<FSpriteModelFrame> SpriteModelFrames;

#define MD3_MAX_SURFACES	32
#define MIN_MODELS	4

struct FSpriteModelFrame
{
	uint8_t modelsAmount = 0;
	TArray<int> modelIDs;
	TArray<FTextureID> skinIDs;
	TArray<FTextureID> surfaceskinIDs;
	TArray<int> modelframes;
	TArray<int> animationIDs;
	float xscale, yscale, zscale;
	// [BB] Added zoffset, rotation parameters and flags.
	// Added xoffset, yoffset
	float xoffset, yoffset, zoffset;
	float xrotate, yrotate, zrotate;
	float rotationCenterX, rotationCenterY, rotationCenterZ;
	float rotationSpeed;
	unsigned int flags;
	const void* type;	// used for hashing, must point to something usable as identifier for the model's owner.
	short sprite;
	short frame;
	int hashnext;
	float angleoffset;
	// added pithoffset, rolloffset.
	float pitchoffset, rolloffset; // I don't want to bother with type transformations, so I made this variables float.
	bool isVoxel;
};


enum ModelRendererType
{
	GLModelRendererType,
	SWModelRendererType,
	PolyModelRendererType,
	NumModelRendererTypes
};

enum EFrameError
{
	FErr_NotFound = -1,
	FErr_Voxel = -2,
	FErr_Singleframe = -3
};

class FModel
{
public:
	FModel();
	virtual ~FModel();

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length) = 0;
	virtual int FindFrame(const char * name, bool nodefault = false) = 0;
	virtual void RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frame, int frame2, double inter, int translation, const FTextureID* surfaceskinids, const TArray<VSMatrix>& boneData, int boneStartPosition) = 0;
	virtual void BuildVertexBuffer(FModelRenderer *renderer) = 0;
	virtual void AddSkins(uint8_t *hitlist, const FTextureID* surfaceskinids) = 0;
	virtual float getAspectFactor(float vscale) { return 1.f; }
	virtual const TArray<TRS>* AttachAnimationData() { return nullptr; };
	virtual const TArray<VSMatrix> CalculateBones(int frame1, int frame2, double inter, const TArray<TRS>* animationData, DBoneComponents* bones, int index) { return {}; };

	void SetVertexBuffer(int type, IModelVertexBuffer *buffer) { mVBuf[type] = buffer; }
	IModelVertexBuffer *GetVertexBuffer(int type) const { return mVBuf[type]; }
	void DestroyVertexBuffer();

	bool hasSurfaces = false;
	FString mFileName;

private:
	IModelVertexBuffer *mVBuf[NumModelRendererTypes];
};

int ModelFrameHash(FSpriteModelFrame* smf);
unsigned FindModel(const char* path, const char* modelfile, bool silent = false);

