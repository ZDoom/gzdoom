#pragma once

#include <stdint.h>
#include "textureid.h"
#include "i_modelvertexbuffer.h"

class FModelRenderer;
class FGameTexture;
class IModelVertexBuffer;
class FModel;
struct FSpriteModelFrame;

FTextureID LoadSkin(const char* path, const char* fn);
void FlushModels();
extern TDeletingArray<FModel*> Models;
extern TArray<FSpriteModelFrame> SpriteModelFrames;

#define MAX_MODELS_PER_FRAME 4
#define MD3_MAX_SURFACES	32

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

int ModelFrameHash(FSpriteModelFrame* smf);
unsigned FindModel(const char* path, const char* modelfile);

