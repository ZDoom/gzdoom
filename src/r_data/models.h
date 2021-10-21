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
	MDL_NOPERPIXELLIGHTING			= 1024, // forces a model to not use per-pixel lighting. useful for voxel-converted-to-model objects.
};

FSpriteModelFrame * FindModelFrame(const PClass * ti, int sprite, int frame, bool dropped);
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
void RenderHUDModel(FModelRenderer* renderer, DPSprite* psp, float ofsX, float ofsY);

#endif
