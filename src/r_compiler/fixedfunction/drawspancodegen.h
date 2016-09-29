
#pragma once

#include "drawercodegen.h"

enum class DrawSpanVariant
{
	Opaque,
	Masked,
	Translucent,
	MaskedTranslucent,
	AddClamp,
	MaskedAddClamp
};

class DrawSpanCodegen : public DrawerCodegen
{
public:
	void Generate(DrawSpanVariant variant, SSAValue args);

private:
	void LoopShade(DrawSpanVariant variant, bool isSimpleShade);
	void LoopFilter(DrawSpanVariant variant, bool isSimpleShade, bool isNearestFilter);
	SSAInt Loop4x(DrawSpanVariant variant, bool isSimpleShade, bool isNearestFilter, bool is64x64);
	void Loop(SSAInt start, DrawSpanVariant variant, bool isSimpleShade, bool isNearestFilter, bool is64x64);
	SSAVec4i Sample(SSAInt xfrac, SSAInt yfrac, bool isNearestFilter, bool is64x64);
	SSAVec4i Shade(SSAVec4i fg, bool isSimpleShade);
	SSAVec4i Blend(SSAVec4i fg, SSAVec4i bg, DrawSpanVariant variant);

	SSAStack<SSAInt> stack_index, stack_xfrac, stack_yfrac;

	SSAUBytePtr destorg;
	SSAUBytePtr source;
	SSAInt destpitch;
	SSAInt xstep;
	SSAInt ystep;
	SSAInt x1;
	SSAInt x2;
	SSAInt y;
	SSAInt xbits;
	SSAInt ybits;
	SSAInt light;
	SSAInt srcalpha;
	SSAInt destalpha;
	SSAInt count;
	SSAUBytePtr data;
	SSAInt yshift;
	SSAInt xshift;
	SSAInt xmask;
	SSABool is_64x64;
	SSABool is_simple_shade;
	SSABool is_nearest_filter;
	SSAShadeConstants shade_constants;
};
