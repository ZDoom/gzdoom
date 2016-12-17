/*
**  DrawTriangle code generation
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

#include "drawercodegen.h"

struct SSATriVertex
{
	SSAFloat x, y, z, w;
	SSAFloat varying[TriVertex::NumVarying];
};

struct SSAStepVariables
{
	SSAFloat W;
	SSAFloat Varying[TriVertex::NumVarying];
};

class DrawTriangleCodegen : public DrawerCodegen
{
public:
	void Generate(TriBlendMode blendmode, bool truecolor, bool colorfill, SSAValue args, SSAValue thread_data);

private:
	void LoadArgs(SSAValue args, SSAValue thread_data);
	SSATriVertex LoadTriVertex(SSAValue v);
	void LoadUniforms(SSAValue uniforms);
	void CalculateGradients();
	SSAFloat FindGradientX(SSAFloat x0, SSAFloat y0, SSAFloat x1, SSAFloat y1, SSAFloat x2, SSAFloat y2, SSAFloat c0, SSAFloat c1, SSAFloat c2);
	SSAFloat FindGradientY(SSAFloat x0, SSAFloat y0, SSAFloat x1, SSAFloat y1, SSAFloat x2, SSAFloat y2, SSAFloat c0, SSAFloat c1, SSAFloat c2);
	void DrawFullSpans(bool isSimpleShade);
	void DrawPartialBlocks(bool isSimpleShade);

	SSAVec4i ProcessPixel32(SSAVec4i bg, SSAInt *varying, bool isSimpleShade);
	SSAInt ProcessPixel8(SSAInt bg, SSAInt *varying);
	SSAVec4i TranslateSample32(SSAInt *varying);
	SSAInt TranslateSample8(SSAInt *varying);
	SSAVec4i Sample32(SSAInt *varying);
	SSAInt Sample8(SSAInt *varying);
	SSAVec4i Shade32(SSAVec4i fg, SSAInt light, bool isSimpleShade);
	SSAInt Shade8(SSAInt c);
	SSAVec4i ToBgra(SSAInt index);
	SSAInt ToPal8(SSAVec4i c);
	SSAVec4i FadeOut(SSAInt frac, SSAVec4i color);

	SSAStack<SSAInt> stack_i, stack_y, stack_x;
	SSAStack<SSAFloat> stack_posYW, stack_posXW;
	SSAStack<SSAFloat> stack_posYVarying[TriVertex::NumVarying];
	SSAStack<SSAFloat> stack_posXVarying[TriVertex::NumVarying];
	SSAStack<SSAInt> stack_varyingPos[TriVertex::NumVarying];
	SSAStack<SSAInt> stack_lightpos;
	SSAStack<SSAUBytePtr> stack_dest;

	SSAStepVariables gradientX, gradientY, start;
	SSAFloat shade, globVis;

	SSAInt currentlight;
	SSAUBytePtr currentcolormap;

	SSAUBytePtr destOrg;
	SSAInt pitch;
	SSATriVertex v1;
	SSATriVertex v2;
	SSATriVertex v3;
	SSAUBytePtr texturePixels;
	SSAInt textureWidth;
	SSAInt textureHeight;
	SSAUBytePtr translation;
	SSAInt color, srcalpha, destalpha;

	SSAInt light;
	SSAShadeConstants shade_constants;
	SSABool is_simple_shade;
	SSABool is_nearest_filter;
	SSABool is_fixed_light;

	SSAUBytePtr Colormaps;
	SSAUBytePtr RGB32k;
	SSAUBytePtr BaseColors;

	SSAInt numSpans;
	SSAInt numBlocks;
	SSAInt startX;
	SSAInt startY;
	SSAValue fullSpans; // TriFullSpan[]
	SSAValue partialBlocks; // TriPartialBlock[]

	TriBlendMode blendmode;
	bool truecolor;
	bool colorfill;
	int pixelsize;
};
