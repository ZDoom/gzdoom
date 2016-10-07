
#pragma once

#include "drawercodegen.h"

enum class DrawColumnVariant
{
	Fill,
	FillAdd,
	FillAddClamp,
	FillSubClamp,
	FillRevSubClamp,
	DrawCopy,
	Draw,
	DrawAdd,
	DrawTranslated,
	DrawTlatedAdd,
	DrawShaded,
	DrawAddClamp,
	DrawAddClampTranslated,
	DrawSubClamp,
	DrawSubClampTranslated,
	DrawRevSubClamp,
	DrawRevSubClampTranslated
};

enum class DrawColumnMethod
{
	Normal,
	Rt1,
	Rt4
};

class DrawColumnCodegen : public DrawerCodegen
{
public:
	void Generate(DrawColumnVariant variant, DrawColumnMethod method, SSAValue args, SSAValue thread_data);

private:
	void Loop(DrawColumnVariant variant, DrawColumnMethod method, bool isSimpleShade);
	SSAVec4i ProcessPixel(SSAInt sample_index, SSAVec4i bgcolor, DrawColumnVariant variant, bool isSimpleShade);
	SSAInt ColormapSample(SSAInt frac);
	SSAInt TranslateSample(SSAInt frac);
	SSAVec4i Shade(SSAInt palIndex, bool isSimpleShade);

	SSAStack<SSAInt> stack_index, stack_frac;

	SSAUBytePtr dest;
	SSAUBytePtr source;
	SSAUBytePtr colormap;
	SSAUBytePtr translation;
	SSAUBytePtr basecolors;
	SSAInt pitch;
	SSAInt count;
	SSAInt dest_y;
	SSAInt iscale;
	SSAInt texturefrac;
	SSAInt light;
	SSAVec4i color;
	SSAVec4i srccolor;
	SSAInt srcalpha;
	SSAInt destalpha;
	SSABool is_simple_shade;
	SSAShadeConstants shade_constants;
	SSAWorkerThread thread;
};
