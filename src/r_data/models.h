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
#include "voxels.h"
#include "i_modelvertexbuffer.h"
#include "model.h"

class FModelRenderer;

struct FSpriteModelFrame;
class IModelVertexBuffer;
struct FLevelLocals;

//
// [BB] Model rendering flags.
//
enum
{
	// [BB] Color translations for the model skin are ignored. This is
	// useful if the skin texture is not using the game palette.
	MDL_IGNORETRANSLATION			= 1<<0,
	MDL_PITCHFROMMOMENTUM			= 1<<1,
	MDL_ROTATING					= 1<<2,
	MDL_INTERPOLATEDOUBLEDFRAMES	= 1<<3,
	MDL_NOINTERPOLATION				= 1<<4,
	MDL_USEACTORPITCH				= 1<<5,
	MDL_USEACTORROLL				= 1<<6,
	MDL_BADROTATION					= 1<<7,
	MDL_DONTCULLBACKFACES			= 1<<8,
	MDL_USEROTATIONCENTER			= 1<<9,
	MDL_NOPERPIXELLIGHTING			= 1<<10, // forces a model to not use per-pixel lighting. useful for voxel-converted-to-model objects.
	MDL_SCALEWEAPONFOV				= 1<<11,	// scale weapon view model with higher user FOVs
	MDL_MODELSAREATTACHMENTS		= 1<<12,	// any model index after 0 is treated as an attachment, and therefore will use the bone results of index 0
	MDL_CORRECTPIXELSTRETCH			= 1<<13,	// ensure model does not distort with pixel stretch when pitch/roll is applied
	MDL_FORCECULLBACKFACES			= 1<<14,
};

FSpriteModelFrame * FindModelFrame(AActor * thing, int sprite, int frame, bool dropped);
FSpriteModelFrame * FindModelFrame(const PClass * ti, bool is_decoupled, int sprite, int frame, bool dropped);
FSpriteModelFrame * FindModelFrame(const PClass * ti, int sprite, int frame, bool dropped);
//FSpriteModelFrame * FindModelFrameRaw(const AActor * actorDefaults, const PClass * ti, int sprite, int frame, bool dropped);

bool IsHUDModelForPlayerAvailable(player_t * player);

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
void RenderHUDModel(FModelRenderer* renderer, DPSprite* psp, FVector3 translation, FVector3 rotation, FVector3 rotation_pivot, FSpriteModelFrame *smf);

struct CalcModelFrameInfo
{
	int smf_flags;
	const FSpriteModelFrame * smfNext;
	float inter;
	bool is_decoupled;
	ModelAnimFrameInterp decoupled_frame;
	ModelAnimFrame * decoupled_frame_prev;
	unsigned modelsamount;
};

struct ModelDrawInfo
{
	TArray<FTextureID> surfaceskinids;
	int modelid;
	int animationid;
	int modelframe;
	int modelframenext;
	FTextureID skinid;
};

class DActorModelData;

CalcModelFrameInfo CalcModelFrame(FLevelLocals *Level, const FSpriteModelFrame *smf, const FState *curState, const int curTics, DActorModelData* modelData, AActor* actor, bool is_decoupled, double tic);

// returns true if the model isn't removed
bool CalcModelOverrides(int modelindex, const FSpriteModelFrame *smf, DActorModelData* modelData, const CalcModelFrameInfo &frameinfo, ModelDrawInfo &drawinfo, bool is_decoupled);

const TArray<VSMatrix> * ProcessModelFrame(FModel * animation, bool nextFrame, int i, const FSpriteModelFrame *smf, DActorModelData* modelData, const CalcModelFrameInfo &frameinfo, ModelDrawInfo &drawinfo, bool is_decoupled, double tic, BoneInfo *out);

EXTERN_CVAR(Float, cl_scaleweaponfov)

#endif
