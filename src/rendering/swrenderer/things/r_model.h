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
#include "models.h"
#include "swrenderer/r_renderthread.h"
#include "swrenderer/things/r_visiblesprite.h"

struct PolyLight;

namespace swrenderer
{
	void RenderHUDModel(RenderThread *thread, DPSprite *psp, float ofsx, float ofsy);

	class RenderModel : public VisibleSprite
	{
	public:
		RenderModel(float x, float y, float z, FSpriteModelFrame *smf, AActor *actor, float idepth);

		static void Project(RenderThread *thread, float x, float y, float z, FSpriteModelFrame *smf, AActor *actor);

	protected:
		void Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor) override;
		bool IsModel() const override { return true; }

	private:
		float x, y, z;
		FSpriteModelFrame *smf;
		AActor *actor;
		VSMatrix WorldToClip;
		bool MirrorWorldToClip;
	};

#if 0
	class SWModelRenderer : public FModelRenderer
	{
	public:
		SWModelRenderer(RenderThread *thread, Fake3DTranslucent clip3DFloor, Mat4f *worldToClip, bool mirrorWorldToClip);

		void AddLights(AActor *actor);

		ModelRendererType GetType() const override { return SWModelRendererType; }

		void BeginDrawModel(FRenderStyle style, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix, bool mirrored) override;
		void EndDrawModel(FRenderStyle style, FSpriteModelFrame *smf) override;
		IModelVertexBuffer *CreateVertexBuffer(bool needindex, bool singleframe) override;
		VSMatrix GetViewToWorldMatrix() override;
		void BeginDrawHUDModel(FRenderStyle style, const VSMatrix &objectToWorldMatrix, bool mirrored) override;
		void EndDrawHUDModel(FRenderStyle style) override;
		void SetInterpolation(double interpolation) override;
		void SetMaterial(FTexture *skin, bool clampNoFilter, int translation) override;
		void DrawArrays(int start, int count) override;
		void DrawElements(int numIndices, size_t offset) override;

		void SetTransform();

		RenderThread *Thread = nullptr;
		Fake3DTranslucent Clip3DFloor;

		FRenderStyle RenderStyle;
		float RenderAlpha;
		sector_t *sector;
		bool fullbrightSprite;
		int lightlevel;
		double visibility;
		uint32_t fillcolor;
		uint32_t Translation;

		Mat4f ObjectToWorld;
		PolyClipPlane ClipTop, ClipBottom;
		FTexture *SkinTexture = nullptr;
		float InterpolationFactor = 0.0;
		Mat4f *WorldToClip = nullptr;
		bool MirrorWorldToClip = false;
		PolyLight *Lights = nullptr;
		int NumLights = 0;
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
		TArray<FModelVertex> mVertexBuffer;
		TArray<unsigned int> mIndexBuffer;
	};
#endif
}
