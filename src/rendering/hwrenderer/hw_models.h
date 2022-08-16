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

#pragma once

#include "tarray.h"
#include "p_pspr.h"
#include "voxels.h"
#include "models.h"
#include "hwrenderer/data/buffers.h"
#include "hw_modelvertexbuffer.h"
#include "modelrenderer.h"

class HWSprite;
struct HWDrawInfo;
class FRenderState;


class FHWModelRenderer : public FModelRenderer
{
	friend class FModelVertexBuffer;
	int modellightindex = -1;
	int boneIndexBase = -1;
	HWDrawInfo *di;
	FRenderState &state;
public:
	FHWModelRenderer(HWDrawInfo *d, FRenderState &st, int mli) : modellightindex(mli), di(d), state(st)
	{}
	ModelRendererType GetType() const override { return GLModelRendererType; }
	void BeginDrawModel(FRenderStyle style, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix, bool mirrored) override;
	void EndDrawModel(FRenderStyle style, FSpriteModelFrame *smf) override;
	IModelVertexBuffer *CreateVertexBuffer(bool needindex, bool singleframe) override;
	VSMatrix GetViewToWorldMatrix() override;
	void BeginDrawHUDModel(FRenderStyle style, const VSMatrix &objectToWorldMatrix, bool mirrored) override;
	void EndDrawHUDModel(FRenderStyle style) override;
	void SetInterpolation(double interpolation) override;
	void SetMaterial(FGameTexture *skin, bool clampNoFilter, int translation) override;
	void DrawArrays(int start, int count) override;
	void DrawElements(int numIndices, size_t offset) override;
	int SetupFrame(FModel *model, unsigned int frame1, unsigned int frame2, unsigned int size, const TArray<VSMatrix>& bones, int boneStartIndex) override;

};

