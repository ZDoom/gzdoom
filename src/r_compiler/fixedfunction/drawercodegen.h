/*
**  Drawer code generation
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
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_bool.h"
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
};
