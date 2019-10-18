/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#pragma once

#include "polyrenderer/drawers/poly_triangle.h"
#include "matrix.h"
#include "r_data/models/models.h"

void PolyRenderModel(PolyRenderThread *thread, const Mat4f &worldToClip, uint32_t stencilValue, float x, float y, float z, FSpriteModelFrame *smf, AActor *actor);
void PolyRenderHUDModel(PolyRenderThread *thread, const Mat4f &worldToClip, uint32_t stencilValue, DPSprite *psp, float ofsx, float ofsy);

class PolyModelRenderer : public FModelRenderer
{
public:
	PolyModelRenderer(PolyRenderThread *thread, const Mat4f &worldToClip, uint32_t stencilValue);

	void AddLights(AActor *actor);

	ModelRendererType GetType() const override { return PolyModelRendererType; }

	void BeginDrawModel(AActor *actor, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix, bool mirrored) override;
	void EndDrawModel(AActor *actor, FSpriteModelFrame *smf) override;
	IModelVertexBuffer *CreateVertexBuffer(bool needindex, bool singleframe) override;
	VSMatrix GetViewToWorldMatrix() override;
	void BeginDrawHUDModel(AActor *actor, const VSMatrix &objectToWorldMatrix, bool mirrored) override;
	void EndDrawHUDModel(AActor *actor) override;
	void SetInterpolation(double interpolation) override;
	void SetMaterial(FTexture *skin, bool clampNoFilter, int translation) override;
	void DrawArrays(int start, int count) override;
	void DrawElements(int numIndices, size_t offset) override;

	void SetTransform();

	PolyRenderThread *Thread = nullptr;
	const Mat4f &WorldToClip;
	uint32_t StencilValue = 0;

	FRenderStyle RenderStyle;
	float RenderAlpha;
	sector_t *sector;
	bool fullbrightSprite;
	int lightlevel;
	double visibility;
	uint32_t fillcolor;
	uint32_t Translation;

	Mat4f ObjectToWorld;
	FSoftwareTexture *SkinTexture = nullptr;
	unsigned int *IndexBuffer = nullptr;
	FModelVertex *VertexBuffer = nullptr;
	float InterpolationFactor = 0.0;
	PolyLight *Lights = nullptr;
	int NumLights = 0;
};

class PolyModelVertexBuffer : public IModelVertexBuffer
{
public:
	PolyModelVertexBuffer(bool needindex, bool singleframe);
	~PolyModelVertexBuffer();

	FModelVertex *LockVertexBuffer(unsigned int size) override;
	void UnlockVertexBuffer() override;

	unsigned int *LockIndexBuffer(unsigned int size) override;
	void UnlockIndexBuffer() override;

	void SetupFrame(FModelRenderer *renderer, unsigned int frame1, unsigned int frame2, unsigned int size) override;

private:
	TArray<FModelVertex> mVertexBuffer;
	TArray<unsigned int> mIndexBuffer;
};
