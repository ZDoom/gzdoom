#pragma once
#pragma once

#include "r_defs.h"
#include "m_fixed.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "r_utility.h"
#include "c_cvars.h"
#include <memory>

class FSWSceneTexture;

class SWSceneDrawer
{
	std::unique_ptr<FTexture> PaletteTexture;
	std::unique_ptr<FSWSceneTexture> FBTexture[2];
	int FBTextureIndex = 0;
	bool FBIsTruecolor = false;
	std::unique_ptr<DSimpleCanvas> Canvas;

public:
	SWSceneDrawer();
	~SWSceneDrawer();

	sector_t *RenderView(player_t *player);
};

