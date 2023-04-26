#pragma once

#include "hw_meshbuilder.h"

class Mesh
{
public:
	void Draw(FRenderState& renderstate);

private:
	void Apply(FRenderState& renderstate, const MeshApplyState& apply);

	TArray<MeshApplyState> mApplys;
	TArray<MeshDrawCommand> mDraws;
	TArray<MeshDrawCommand> mIndexedDraws;
	TArray<FFlatVertex> mVertices;
	std::unique_ptr<IBuffer> mVertexBuffer;

	friend class MeshBuilder;
};
