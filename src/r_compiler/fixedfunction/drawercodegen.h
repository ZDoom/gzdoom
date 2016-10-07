
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

class SSAWorkerThread
{
public:
	SSAInt core;
	SSAInt num_cores;
	SSAInt pass_start_y;
	SSAInt pass_end_y;
	SSAUBytePtr temp;
};

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
	// Checks if a line is rendered by this thread
	SSABool line_skipped_by_thread(SSAInt line, SSAWorkerThread thread);

	// The number of lines to skip to reach the first line to be rendered by this thread
	SSAInt skipped_by_thread(SSAInt first_line, SSAWorkerThread thread);

	// The number of lines to be rendered by this thread
	SSAInt count_for_thread(SSAInt first_line, SSAInt count, SSAWorkerThread thread);

	// Calculate the dest address for the first line to be rendered by this thread
	SSAUBytePtr dest_for_thread(SSAInt first_line, SSAInt pitch, SSAUBytePtr dest, SSAWorkerThread thread);

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
