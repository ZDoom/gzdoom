/*
**  Handling drawing a player sprite
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

#include "r_defs.h"

class PolyScreenSprite;
class DPSprite;
struct FSWColormap;

class RenderPolyPlayerSprites
{
public:
	void Render();
	void RenderRemainingSprites();

private:
	void RenderSprite(DPSprite *sprite, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac);

	const int BaseXCenter = 160;
	const int BaseYCenter = 100;

	std::vector<PolyScreenSprite> ScreenSprites;
};

// DScreen accelerated sprite to be rendered
class PolyScreenSprite
{
public:
	void Render();

	FTexture *Pic = nullptr;
	double X1 = 0.0;
	double Y1 = 0.0;
	double Width = 0.0;
	double Height = 0.0;
	FRemapTable *Translation = nullptr;
	bool Flip = false;
	float Alpha = 1;
	FRenderStyle RenderStyle;
	FSWColormap *BaseColormap = nullptr;
	int ColormapNum = 0;
	uint32_t FillColor = 0;
	FDynamicColormap *Colormap = nullptr;
};
