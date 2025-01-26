#pragma once
#include "renderstyle.h"
#include "matrix.h"
#include "model.h"

class FModelRenderer
{
public:
	virtual ~FModelRenderer() = default;

	virtual ModelRendererType GetType() const = 0;

	virtual void BeginDrawModel(FRenderStyle style, int smf_flags, const VSMatrix &objectToWorldMatrix, bool mirrored) = 0;
	virtual void EndDrawModel(FRenderStyle style, int smf_flags) = 0;

	virtual IModelVertexBuffer *CreateVertexBuffer(bool needindex, bool singleframe) = 0;

	virtual VSMatrix GetViewToWorldMatrix() = 0;

	virtual void BeginDrawHUDModel(FRenderStyle style, const VSMatrix &objectToWorldMatrix, bool mirrored, int smf_flags) = 0;
	virtual void EndDrawHUDModel(FRenderStyle style, int smf_flags) = 0;

	virtual void SetInterpolation(double interpolation) = 0;
	virtual void SetMaterial(FGameTexture *skin, bool clampNoFilter, FTranslationID translation) = 0;
	virtual void DrawArrays(int start, int count) = 0;
	virtual void DrawElements(int numIndices, size_t offset) = 0;
	virtual void SetupFrame(FModel* model, unsigned int frame1, unsigned int frame2, unsigned int size, int boneStartIndex) {};
};

