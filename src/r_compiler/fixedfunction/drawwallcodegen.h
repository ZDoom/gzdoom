
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
	void Generate(DrawWallVariant variant, bool fourColumns, SSAValue args, SSAValue thread_data);

private:
	void LoopShade(DrawWallVariant variant, bool fourColumns, bool isSimpleShade);
	void Loop(DrawWallVariant variant, bool fourColumns, bool isSimpleShade, bool isNearestFilter);
	SSAVec4i Sample(SSAInt frac, int index, bool isNearestFilter);
	SSAVec4i Shade(SSAVec4i fg, int index, bool isSimpleShade);
	SSAVec4i Blend(SSAVec4i fg, SSAVec4i bg, DrawWallVariant variant);

	SSAStack<SSAInt> stack_index, stack_frac[4];

	SSAUBytePtr dest;
	SSAUBytePtr source[4];
	SSAUBytePtr source2[4];
	SSAInt pitch;
	SSAInt count;
	SSAInt dest_y;
	SSAInt texturefrac[4];
	SSAInt texturefracx[4];
	SSAInt iscale[4];
	SSAInt textureheight[4];
	SSAInt light[4];
	SSAInt srcalpha;
	SSAInt destalpha;
	SSABool is_simple_shade;
	SSABool is_nearest_filter;
	SSAShadeConstants shade_constants;
	SSAWorkerThread thread;

	SSAInt fracstep[4];
	SSAInt one[4];
};
