/*
**  DrawSky code generation
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

enum class DrawSkyVariant
{
	Single,
	Double
};

class DrawSkyCodegen : public DrawerCodegen
{
public:
	void Generate(DrawSkyVariant variant, SSAValue args, SSAValue thread_data);

private:
	void Loop(DrawSkyVariant variant);
	SSAVec4i Sample(SSAInt frac, DrawSkyVariant variant);
	SSAVec4i FadeOut(SSAInt frac, SSAVec4i color);

	SSAStack<SSAInt> stack_index, stack_frac;

	SSAUBytePtr dest;
	SSAUBytePtr source0;
	SSAUBytePtr source1;
	SSAInt pitch;
	SSAInt count;
	SSAInt dest_y;
	SSAInt texturefrac;
	SSAInt iscale;
	SSAInt textureheight0;
	SSAInt maxtextureheight1;
	SSAVec4i top_color;
	SSAVec4i bottom_color;
	SSAWorkerThread thread;

	SSAInt fracstep;
};
