
#include "models.h"
#include "actor.h"
#include "p_pspr.h"
#include "info.h"
#include "g_levellocals.h"

class FModelRenderer
{
public:
	virtual ~FModelRenderer() { }

	void RenderModel(float x, float y, float z, FSpriteModelFrame *modelframe, AActor *actor, double ticFrac);
	void RenderHUDModel(DPSprite *psp, float ofsx, float ofsy);

	virtual ModelRendererType GetType() const = 0;

	virtual void BeginDrawModel(FRenderStyle style, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix, bool mirrored) = 0;
	virtual void EndDrawModel(FRenderStyle style, FSpriteModelFrame *smf) = 0;

	virtual IModelVertexBuffer *CreateVertexBuffer(bool needindex, bool singleframe) = 0;

	virtual VSMatrix GetViewToWorldMatrix() = 0;

	virtual void BeginDrawHUDModel(FRenderStyle style, const VSMatrix &objectToWorldMatrix, bool mirrored) = 0;
	virtual void EndDrawHUDModel(FRenderStyle style) = 0;

	virtual void SetInterpolation(double interpolation) = 0;
	virtual void SetMaterial(FGameTexture *skin, bool clampNoFilter, int translation) = 0;
	virtual void DrawArrays(int start, int count) = 0;
	virtual void DrawElements(int numIndices, size_t offset) = 0;
	virtual void SetupFrame(FModel *model, unsigned int frame1, unsigned int frame2, unsigned int size) = 0;

private:
	void RenderFrameModels(FLevelLocals *Level, const FSpriteModelFrame *smf, const FState *curState, const int curTics, const PClass *ti, int translation);
};

