
#pragma once

#include "polyrenderer/backend/poly_buffers.h"
#include "rendering/polyrenderer/drawers/poly_triangle.h"

#include "name.h"

#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include "hwrenderer/textures/hw_material.h"

struct HWViewpointUniforms;

class PolyRenderState : public FRenderState
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

	void Bind(PolyDataBuffer *buffer, uint32_t offset, uint32_t length);

private:
	void Apply();
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

	bool mDepthClamp = true;
	PolyDrawArgs args;
};
