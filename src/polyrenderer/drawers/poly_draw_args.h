/*
**  Triangle drawers
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

#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "screen_triangle.h"

class FTexture;

enum class TriangleDrawMode
{
	Normal,
	Fan,
	Strip
};

struct TriDrawTriangleArgs;
struct TriMatrix;

class PolyDrawArgs
{
public:
	TriUniforms uniforms;
	const TriMatrix *objectToClip = nullptr;
	const TriVertex *vinput = nullptr;
	int vcount = 0;
	TriangleDrawMode mode = TriangleDrawMode::Normal;
	bool ccw = false;
	// bool stencilTest = true; // Always true for now
	bool subsectorTest = false;
	bool writeStencil = true;
	bool writeColor = true;
	bool writeSubsector = true;
	const uint8_t *texturePixels = nullptr;
	int textureWidth = 0;
	int textureHeight = 0;
	const uint8_t *translation = nullptr;
	uint8_t stenciltestvalue = 0;
	uint8_t stencilwritevalue = 0;
	const uint8_t *colormaps = nullptr;
	float clipPlane[4];
	TriBlendMode blendmode = TriBlendMode::Copy;

	void SetClipPlane(float a, float b, float c, float d);
	void SetTexture(FTexture *texture);
	void SetTexture(FTexture *texture, uint32_t translationID, bool forcePal = false);
	void SetColormap(FSWColormap *base_colormap);
};
