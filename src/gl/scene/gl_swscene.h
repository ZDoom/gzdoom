#pragma once
#pragma once

#include "r_defs.h"
#include "m_fixed.h"
#include "gl_clipper.h"
#include "gl_portal.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderer.h"
#include "r_utility.h"
#include "c_cvars.h"

class FSWSceneTexture;

class SWSceneDrawer
{
	FTexture *PaletteTexture = nullptr;
	FSWSceneTexture *FBTexture = nullptr;
	bool FBIsTruecolor = false;

	
	void BlendView (player_t *CPlayer, float blend[4]);
	bool CreateResources();
	void BindFBBuffer();


public:
	SWSceneDrawer();
	~SWSceneDrawer();

	void RenderView(player_t *player);
};

