/*
**  DrawWall code generation
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

enum class DrawWallVariant
{
	Opaque,
	Masked,
	Add,
	AddClamp,
	SubClamp,
	RevSubClamp
};

class DrawWallCodegen : public DrawerCodegen
{
public:
	void Generate(DrawWallVariant variant, SSAValue args, SSAValue thread_data);

private:
	void LoopShade(DrawWallVariant variant, bool isSimpleShade);
	void Loop(DrawWallVariant variant, bool isSimpleShade, bool isNearestFilter);
	SSAVec4i Sample(SSAInt frac, bool isNearestFilter);
	SSAVec4i SampleLinear(SSAUBytePtr col0, SSAUBytePtr col1, SSAInt texturefracx, SSAInt texturefracy, SSAInt one, SSAInt height);
	SSAVec4i Shade(SSAVec4i fg, bool isSimpleShade);
	SSAVec4i Blend(SSAVec4i fg, SSAVec4i bg, DrawWallVariant variant);

	SSAStack<SSAInt> stack_index, stack_frac, stack_light_index;
	SSAStack<SSAVec4i> stack_lit_color;
	SSAStack<SSAFloat> stack_z;

	SSAUBytePtr dest;
	SSAUBytePtr source;
	SSAUBytePtr source2;
	SSAInt pitch;
	SSAInt count;
	SSAInt dest_y;
	SSAInt texturefrac;
	SSAInt texturefracx;
	SSAInt iscale;
	SSAInt textureheight;
	SSAInt light;
	SSAInt srcalpha;
	SSAInt destalpha;
	SSABool is_simple_shade;
	SSABool is_nearest_filter;
	SSAShadeConstants shade_constants;
	SSAWorkerThread thread;

	SSAInt fracstep;
	SSAInt one;

	SSAFloat start_z, step_z;

	SSAValue dynlights; // TriLight*
	SSAInt num_dynlights;
	SSAFloat z;
};
