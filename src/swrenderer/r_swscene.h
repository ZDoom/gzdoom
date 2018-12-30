#pragma once
#pragma once

#include "r_defs.h"
#include "m_fixed.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "r_utility.h"
#include "c_cvars.h"
#include <memory>

class FWrapperTexture;

class SWSceneDrawer
{
	FTexture *PaletteTexture;
	std::unique_ptr<FWrapperTexture> FBTexture[2];
	int FBTextureIndex = 0;
	bool FBIsTruecolor = false;
	std::unique_ptr<DCanvas> Canvas;

public:
	SWSceneDrawer();
	~SWSceneDrawer();

	sector_t *RenderView(player_t *player);
};

