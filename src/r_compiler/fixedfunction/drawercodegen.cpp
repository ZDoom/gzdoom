
#include "i_system.h"
#include "r_compiler/llvm_include.h"
#include "r_compiler/fixedfunction/drawercodegen.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"

SSABool DrawerCodegen::line_skipped_by_thread(SSAInt line, SSAWorkerThread thread)
{
	return line < thread.pass_start_y || line >= thread.pass_end_y || !(line % thread.num_cores == thread.core);
}

SSAInt DrawerCodegen::skipped_by_thread(SSAInt first_line, SSAWorkerThread thread)
{
	SSAInt pass_skip = SSAInt::MAX(thread.pass_start_y - first_line, SSAInt(0));
	SSAInt core_skip = (thread.num_cores - (first_line + pass_skip - thread.core) % thread.num_cores) % thread.num_cores;
	return pass_skip + core_skip;
}

SSAInt DrawerCodegen::count_for_thread(SSAInt first_line, SSAInt count, SSAWorkerThread thread)
{
	SSAInt lines_until_pass_end = SSAInt::MAX(thread.pass_end_y - first_line, SSAInt(0));
	count = SSAInt::MIN(count, lines_until_pass_end);
	SSAInt c = (count - skipped_by_thread(first_line, thread) + thread.num_cores - 1) / thread.num_cores;
	return SSAInt::MAX(c, SSAInt(0));
}

SSAUBytePtr DrawerCodegen::dest_for_thread(SSAInt first_line, SSAInt pitch, SSAUBytePtr dest, SSAWorkerThread thread)
{
	return dest[skipped_by_thread(first_line, thread) * pitch * 4];
}

SSAInt DrawerCodegen::calc_light_multiplier(SSAInt light)
{
	return 256 - (light >> (FRACBITS - 8));
}

SSAVec4i DrawerCodegen::shade_pal_index_simple(SSAInt index, SSAInt light, SSAUBytePtr basecolors)
{
	SSAVec4i color = basecolors[index * 4].load_vec4ub(true); // = GPalette.BaseColors[index];
	return shade_bgra_simple(color, light);
}

SSAVec4i DrawerCodegen::shade_pal_index_advanced(SSAInt index, SSAInt light, const SSAShadeConstants &constants, SSAUBytePtr basecolors)
{
	SSAVec4i color = basecolors[index * 4].load_vec4ub(true); // = GPalette.BaseColors[index];
	return shade_bgra_advanced(color, light, constants);
}

SSAVec4i DrawerCodegen::shade_bgra_simple(SSAVec4i color, SSAInt light)
{
	SSAInt alpha = color[3];
	color = color * light / 256;
	return color.insert(3, alpha);
}

SSAVec4i DrawerCodegen::shade_bgra_advanced(SSAVec4i color, SSAInt light, const SSAShadeConstants &constants)
{
	SSAInt blue = color[0];
	SSAInt green = color[1];
	SSAInt red = color[2];
	SSAInt alpha = color[3];

	SSAInt intensity = ((red * 77 + green * 143 + blue * 37) >> 8) * constants.desaturate;

	SSAVec4i inv_light = 256 - light;
	SSAVec4i inv_desaturate = 256 - constants.desaturate;

	color = (color * inv_desaturate + intensity) / 256;
	color = (constants.fade * inv_light + color * light) / 256;
	color = (color * constants.light) / 256;

	return color.insert(3, alpha);
}

SSAVec4i DrawerCodegen::blend_copy(SSAVec4i fg)
{
	return fg;
}

SSAVec4i DrawerCodegen::blend_add(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha)
{
	SSAVec4i color = (fg * srcalpha + bg * destalpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i DrawerCodegen::blend_sub(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha)
{
	SSAVec4i color = (bg * destalpha - fg * srcalpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i DrawerCodegen::blend_revsub(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha)
{
	SSAVec4i color = (fg * srcalpha - bg * destalpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i DrawerCodegen::blend_alpha_blend(SSAVec4i fg, SSAVec4i bg)
{
	SSAInt alpha = fg[3];
	alpha = alpha + (alpha >> 7); // // 255 -> 256
	SSAInt inv_alpha = 256 - alpha;
	SSAVec4i color = (fg * alpha + bg * inv_alpha) / 256;
	return color.insert(3, 255);
}

SSAInt DrawerCodegen::calc_blend_bgalpha(SSAVec4i fg, SSAInt destalpha)
{
	SSAInt alpha = fg[3];
	alpha = alpha + (alpha >> 7);
	SSAInt inv_alpha = 256 - alpha;
	return (destalpha * alpha + 256 * inv_alpha + 128) >> 8;
}

SSAVec4i DrawerCodegen::sample_linear(SSAUBytePtr col0, SSAUBytePtr col1, SSAInt texturefracx, SSAInt texturefracy, SSAInt one, SSAInt height)
{
	SSAInt frac_y0 = (texturefracy >> FRACBITS) * height;
	SSAInt frac_y1 = ((texturefracy + one) >> FRACBITS) * height;
	SSAInt y0 = frac_y0 >> FRACBITS;
	SSAInt y1 = frac_y1 >> FRACBITS;

	SSAVec4i p00 = col0[y0 * 4].load_vec4ub(true);
	SSAVec4i p01 = col0[y1 * 4].load_vec4ub(true);
	SSAVec4i p10 = col1[y0 * 4].load_vec4ub(true);
	SSAVec4i p11 = col1[y1 * 4].load_vec4ub(true);

	SSAInt inv_b = texturefracx;
	SSAInt a = (frac_y1 >> (FRACBITS - 4)) & 15;
	SSAInt inv_a = 16 - a;
	SSAInt b = 16 - inv_b;

	return (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
}

SSAVec4i DrawerCodegen::sample_linear(SSAUBytePtr texture, SSAInt xfrac, SSAInt yfrac, SSAInt xbits, SSAInt ybits)
{
	SSAInt xshift = (32 - xbits);
	SSAInt yshift = (32 - ybits);
	SSAInt xmask = (SSAInt(1) << xshift) - 1;
	SSAInt ymask = (SSAInt(1) << yshift) - 1;
	SSAInt x = xfrac >> xbits;
	SSAInt y = yfrac >> ybits;

	SSAVec4i p00 = texture[((y & ymask) + ((x & xmask) << yshift)) * 4].load_vec4ub(true);
	SSAVec4i p01 = texture[(((y + 1) & ymask) + ((x & xmask) << yshift)) * 4].load_vec4ub(true);
	SSAVec4i p10 = texture[((y & ymask) + (((x + 1) & xmask) << yshift)) * 4].load_vec4ub(true);
	SSAVec4i p11 = texture[(((y + 1) & ymask) + (((x + 1) & xmask) << yshift)) * 4].load_vec4ub(true);

	SSAInt inv_b = (xfrac >> (xbits - 4)) & 15;
	SSAInt inv_a = (yfrac >> (ybits - 4)) & 15;
	SSAInt a = 16 - inv_a;
	SSAInt b = 16 - inv_b;

	return (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
}
