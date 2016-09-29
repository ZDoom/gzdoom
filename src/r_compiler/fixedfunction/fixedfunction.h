
#pragma once

#include "r_compiler/llvmdrawers.h"
#include "r_compiler/ssa/ssa_value.h"
#include "r_compiler/ssa/ssa_vec4f.h"
#include "r_compiler/ssa/ssa_vec4i.h"
#include "r_compiler/ssa/ssa_vec8s.h"
#include "r_compiler/ssa/ssa_vec16ub.h"
#include "r_compiler/ssa/ssa_int.h"
#include "r_compiler/ssa/ssa_int_ptr.h"
#include "r_compiler/ssa/ssa_short.h"
#include "r_compiler/ssa/ssa_ubyte_ptr.h"
#include "r_compiler/ssa/ssa_vec4f_ptr.h"
#include "r_compiler/ssa/ssa_vec4i_ptr.h"
#include "r_compiler/ssa/ssa_pixels.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_barycentric_weight.h"
#include "r_compiler/llvm_include.h"

class SSAShadeConstants
{
public:
	SSAVec4i light;
	SSAVec4i fade;
	SSAInt desaturate;
};

class DrawerCodegen
{
public:
	// LightBgra
	SSAInt calc_light_multiplier(SSAInt light);
	SSAVec4i shade_pal_index_simple(SSAInt index, SSAInt light, SSAUBytePtr basecolors);
	SSAVec4i shade_pal_index_advanced(SSAInt index, SSAInt light, const SSAShadeConstants &constants, SSAUBytePtr basecolors);
	SSAVec4i shade_bgra_simple(SSAVec4i color, SSAInt light);
	SSAVec4i shade_bgra_advanced(SSAVec4i color, SSAInt light, const SSAShadeConstants &constants);

	// BlendBgra
	SSAVec4i blend_copy(SSAVec4i fg);
	SSAVec4i blend_add(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha);
	SSAVec4i blend_sub(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha);
	SSAVec4i blend_revsub(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha);
	SSAVec4i blend_alpha_blend(SSAVec4i fg, SSAVec4i bg);

	// Calculates the final alpha values to be used when combined with the source texture alpha channel
	SSAInt calc_blend_bgalpha(SSAVec4i fg, SSAInt destalpha);

	// SampleBgra
	SSAVec4i sample_linear(SSAUBytePtr col0, SSAUBytePtr col1, SSAInt texturefracx, SSAInt texturefracy, SSAInt one, SSAInt height);
	SSAVec4i sample_linear(SSAUBytePtr texture, SSAInt xfrac, SSAInt yfrac, SSAInt xbits, SSAInt ybits);
};

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
