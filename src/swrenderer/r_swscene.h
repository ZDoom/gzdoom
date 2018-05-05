#pragma once
#pragma once

#include "r_defs.h"
#include "m_fixed.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "r_utility.h"
#include "c_cvars.h"

class FSWSceneTexture;

class SWSceneDrawer
{
	FTexture *PaletteTexture = nullptr;
	FSWSceneTexture *FBTexture = nullptr;
	bool FBIsTruecolor = false;

public:
	SWSceneDrawer();
	~SWSceneDrawer();

	sector_t *RenderView(player_t *player);
};

