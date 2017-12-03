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
#include "r_data/matrix.h"
#include "r_data/models/models.h"
#include "swrenderer/r_renderthread.h"
#include "swrenderer/things/r_visiblesprite.h"

namespace swrenderer
{
	void RenderHUDModel(RenderThread *thread, DPSprite *psp, float ofsx, float ofsy);

	class RenderModel : public VisibleSprite
	{
	public:
		RenderModel(float x, float y, float z, FSpriteModelFrame *smf, AActor *actor, float idepth);

		static void Project(RenderThread *thread, float x, float y, float z, FSpriteModelFrame *smf, AActor *actor);

	protected:
		void Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ) override;
		bool IsModel() const override { return true; }

	private:
		float x, y, z;
		FSpriteModelFrame *smf;
		AActor *actor;
	};

	class SWModelRenderer : public FModelRenderer
	{
	public:
		SWModelRenderer(RenderThread *thread);

		void BeginDrawModel(AActor *actor, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix) override;
		void EndDrawModel(AActor *actor, FSpriteModelFrame *smf) override;
		IModelVertexBuffer *CreateVertexBuffer(bool needindex, bool singleframe) override;
		void SetVertexBuffer(IModelVertexBuffer *buffer) override;
		void ResetVertexBuffer() override;
		VSMatrix GetViewToWorldMatrix() override;
		void BeginDrawHUDModel(AActor *actor, const VSMatrix &objectToWorldMatrix) override;
		void EndDrawHUDModel(AActor *actor) override;
		void SetInterpolation(double interpolation) override;
		void SetMaterial(FTexture *skin, bool clampNoFilter, int translation) override;
		void DrawArrays(int start, int count) override;
		void DrawElements(int numIndices, size_t offset) override;
		double GetTimeFloat() override;

		RenderThread *Thread = nullptr;

		AActor *ModelActor = nullptr;
		TriMatrix ObjectToWorld;
		FTexture *SkinTexture = nullptr;
		unsigned int *IndexBuffer = nullptr;
		TriVertex *VertexBuffer = nullptr;
		float InterpolationFactor = 0.0;
	};

	class SWModelVertexBuffer : public IModelVertexBuffer
	{
	public:
		SWModelVertexBuffer(bool needindex, bool singleframe);
		~SWModelVertexBuffer();

		FModelVertex *LockVertexBuffer(unsigned int size) override;
		void UnlockVertexBuffer() override;

		unsigned int *LockIndexBuffer(unsigned int size) override;
		void UnlockIndexBuffer() override;

		void SetupFrame(FModelRenderer *renderer, unsigned int frame1, unsigned int frame2, unsigned int size) override;

	private:
		int mIndexFrame[2];
		TArray<FModelVertex> mVertexBuffer;
		TArray<unsigned int> mIndexBuffer;
	};
}
