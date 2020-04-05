
#pragma once

#include "polyrenderer/backend/poly_buffers.h"
#include "rendering/polyrenderer/drawers/poly_triangle.h"

#include "name.h"

#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include "hwrenderer/textures/hw_material.h"

struct HWViewpointUniforms;

class PolyRenderState final : public FRenderState
{
public:
	PolyRenderState();

	// Draw commands
	void ClearScreen() override;
	void Draw(int dt, int index, int count, bool apply = true) override;
	void DrawIndexed(int dt, int index, int count, bool apply = true) override;

	// Immediate render state change commands. These only change infrequently and should not clutter the render state.
	bool SetDepthClamp(bool on) override;
	void SetDepthMask(bool on) override;
	void SetDepthFunc(int func) override;
	void SetDepthRange(float min, float max) override;
	void SetColorMask(bool r, bool g, bool b, bool a) override;
	void SetStencil(int offs, int op, int flags = -1) override;
	void SetCulling(int mode) override;
	void EnableClipDistance(int num, bool state) override;
	void Clear(int targets) override;
	void EnableStencil(bool on) override;
	void SetScissor(int x, int y, int w, int h) override;
	void SetViewport(int x, int y, int w, int h) override;
	void EnableDepthTest(bool on) override;
	void EnableMultisampling(bool on) override;
	void EnableLineSmooth(bool on) override;
	void EnableDrawBuffers(int count) override;

	void SetRenderTarget(DCanvas *canvas, PolyDepthStencil *depthStencil, bool topdown);
	void Bind(PolyDataBuffer *buffer, uint32_t offset, uint32_t length);
	PolyVertexInputAssembly *GetVertexFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs);
	void EndRenderPass();

	void SetColormapShader(bool enable);

private:
	void Apply();
	void ApplyMaterial();
	void ApplyMatrices();

	struct Matrices
	{
		VSMatrix ModelMatrix;
		VSMatrix NormalModelMatrix;
		VSMatrix TextureMatrix;
	} mMatrices;
	VSMatrix mIdentityMatrix;
	bool mFirstMatrixApply = true;

	HWViewpointUniforms *mViewpointUniforms = nullptr;
	std::vector<std::unique_ptr<PolyVertexInputAssembly>> mVertexFormats;

	bool mDepthClamp = true;
	int mTempTM = TM_NORMAL;

	struct RenderTarget
	{
		DCanvas *Canvas = nullptr;
		PolyDepthStencil *DepthStencil = nullptr;
		bool TopDown = true;
	} mRenderTarget;

	struct Rect
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
	} mScissor, mViewport;

	bool mNeedApply = true;

	bool mDepthTest = false;
	bool mDepthMask = false;
	int mDepthFunc = DF_Always;
	float mDepthRangeMin = 0.0f;
	float mDepthRangeMax = 1.0f;
	bool mStencilEnabled = false;
	int mStencilValue = 0;
	int mStencilOp = SOP_Keep;
	int mCulling = Cull_None;
	bool mColorMask[4] = { true, true, true, true };
	bool mColormapShader = false;

	PolyCommandBuffer* mDrawCommands = nullptr;
};
