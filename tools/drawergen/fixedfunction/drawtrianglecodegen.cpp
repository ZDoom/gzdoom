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

#include "precomp.h"
#include "timestamp.h"
#include "fixedfunction/drawtrianglecodegen.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_scope.h"
#include "ssa/ssa_for_block.h"
#include "ssa/ssa_if_block.h"
#include "ssa/ssa_stack.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_struct_type.h"
#include "ssa/ssa_value.h"

void DrawTriangleCodegen::Generate(TriBlendMode blendmode, bool truecolor, bool colorfill, SSAValue args, SSAValue thread_data)
{
	this->blendmode = blendmode;
	this->truecolor = truecolor;
	this->colorfill = colorfill;
	pixelsize = truecolor ? 4 : 1;

	LoadArgs(args, thread_data);
	CalculateGradients();

	if (truecolor)
	{
		SSAIfBlock branch;
		branch.if_block(is_simple_shade);
		{
			DrawFullSpans(true);
			DrawPartialBlocks(true);
		}
		branch.else_block();
		{
			DrawFullSpans(false);
			DrawPartialBlocks(false);
		}
		branch.end_block();
	}
	else
	{
		DrawFullSpans(true);
		DrawPartialBlocks(true);
	}
}

void DrawTriangleCodegen::DrawFullSpans(bool isSimpleShade)
{
	stack_i.store(SSAInt(0));
	SSAForBlock loop;
	SSAInt i = stack_i.load();
	loop.loop_block(i < numSpans, 0);
	{
		SSAInt spanX = SSAShort(fullSpans[i][0].load(true).v).zext_int();
		SSAInt spanY = SSAShort(fullSpans[i][1].load(true).v).zext_int();
		SSAInt spanLength = fullSpans[i][2].load(true);

		SSAInt width = spanLength;
		SSAInt height = SSAInt(8);

		stack_dest.store(destOrg[(spanX + spanY * pitch) * pixelsize]);
		stack_posYW.store(start.W + gradientX.W * (spanX - startX) + gradientY.W * (spanY - startY));
		for (int j = 0; j < TriVertex::NumVarying; j++)
			stack_posYVarying[j].store(start.Varying[j] + gradientX.Varying[j] * (spanX - startX) + gradientY.Varying[j] * (spanY - startY));
		stack_y.store(SSAInt(0));

		SSAForBlock loop_y;
		SSAInt y = stack_y.load();
		SSAUBytePtr dest = stack_dest.load();
		SSAStepVariables blockPosY;
		blockPosY.W = stack_posYW.load();
		for (int j = 0; j < TriVertex::NumVarying; j++)
			blockPosY.Varying[j] = stack_posYVarying[j].load();
		loop_y.loop_block(y < height, 0);
		{
			stack_posXW.store(blockPosY.W);
			for (int j = 0; j < TriVertex::NumVarying; j++)
				stack_posXVarying[j].store(blockPosY.Varying[j]);

			SSAFloat rcpW = SSAFloat((float)0x01000000) / blockPosY.W;
			stack_lightpos.store(FRACUNIT - SSAInt(SSAFloat::clamp(shade - SSAFloat::MIN(SSAFloat(24.0f / 32.0f), globVis * blockPosY.W), SSAFloat(0.0f), SSAFloat(31.0f / 32.0f)) * (float)FRACUNIT, true));
			for (int j = 0; j < TriVertex::NumVarying; j++)
				stack_varyingPos[j].store(SSAInt(blockPosY.Varying[j] * rcpW, false));
			stack_x.store(SSAInt(0));
			
			SSAForBlock loop_x;
			SSAInt x = stack_x.load();
			SSAStepVariables blockPosX;
			blockPosX.W = stack_posXW.load();
			for (int j = 0; j < TriVertex::NumVarying; j++)
				blockPosX.Varying[j] = stack_posXVarying[j].load();
			SSAInt lightpos = stack_lightpos.load();
			SSAInt varyingPos[TriVertex::NumVarying];
			for (int j = 0; j < TriVertex::NumVarying; j++)
				varyingPos[j] = stack_varyingPos[j].load();
			loop_x.loop_block(x < width, 0);
			{
				blockPosX.W = blockPosX.W + gradientX.W * 8.0f;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosX.Varying[j] = blockPosX.Varying[j] + gradientX.Varying[j] * 8.0f;
				
				rcpW = SSAFloat((float)0x01000000) / blockPosX.W;
				SSAInt varyingStep[TriVertex::NumVarying];
				for (int j = 0; j < TriVertex::NumVarying; j++)
				{
					SSAInt nextPos = SSAInt(blockPosX.Varying[j] * rcpW, false);
					varyingStep[j] = (nextPos - varyingPos[j]) / 8;
				}

				SSAInt lightnext = FRACUNIT - SSAInt(SSAFloat::clamp(shade - SSAFloat::MIN(SSAFloat(24.0f / 32.0f), globVis * blockPosX.W), SSAFloat(0.0f), SSAFloat(31.0f / 32.0f)) * (float)FRACUNIT, true);
				SSAInt lightstep = (lightnext - lightpos) / 8;

				if (truecolor)
				{
					for (int ix = 0; ix < 8; ix += 4)
					{
						SSAUBytePtr destptr = dest[(x * 8 + ix) * 4];
						SSAVec16ub pixels16 = destptr.load_unaligned_vec16ub(false);
						SSAVec8s pixels8hi = SSAVec8s::extendhi(pixels16);
						SSAVec8s pixels8lo = SSAVec8s::extendlo(pixels16);
						SSAVec4i pixels[4] =
						{
							SSAVec4i::extendlo(pixels8lo),
							SSAVec4i::extendhi(pixels8lo),
							SSAVec4i::extendlo(pixels8hi),
							SSAVec4i::extendhi(pixels8hi)
						};

						for (int sse = 0; sse < 4; sse++)
						{
							currentlight = is_fixed_light.select(light, lightpos >> 8);
							pixels[sse] = ProcessPixel32(pixels[sse], varyingPos, isSimpleShade);

							for (int j = 0; j < TriVertex::NumVarying; j++)
								varyingPos[j] = varyingPos[j] + varyingStep[j];
							lightpos = lightpos + lightstep;
						}

						destptr.store_unaligned_vec16ub(SSAVec16ub(SSAVec8s(pixels[0], pixels[1]), SSAVec8s(pixels[2], pixels[3])));
					}
				}
				else
				{
					for (int ix = 0; ix < 8; ix++)
					{
						currentlight = is_fixed_light.select(light, lightpos >> 8);
						SSAInt colormapindex = SSAInt::MIN((256 - currentlight) * 32 / 256, SSAInt(31));
						currentcolormap = Colormaps[colormapindex << 8];

						SSAUBytePtr destptr = dest[(x * 8 + ix)];
						destptr.store(ProcessPixel8(destptr.load(false).zext_int(), varyingPos).trunc_ubyte());

						for (int j = 0; j < TriVertex::NumVarying; j++)
							varyingPos[j] = varyingPos[j] + varyingStep[j];
						lightpos = lightpos + lightstep;
					}
				}
				
				for (int j = 0; j < TriVertex::NumVarying; j++)
					stack_varyingPos[j].store(varyingPos[j]);
				stack_lightpos.store(lightpos);
				stack_posXW.store(blockPosX.W);
				for (int j = 0; j < TriVertex::NumVarying; j++)
					stack_posXVarying[j].store(blockPosX.Varying[j]);
				stack_x.store(x + 1);
			}
			loop_x.end_block();

			stack_posYW.store(blockPosY.W + gradientY.W);
			for (int j = 0; j < TriVertex::NumVarying; j++)
				stack_posYVarying[j].store(blockPosY.Varying[j] + gradientY.Varying[j]);
			stack_dest.store(dest[pitch * pixelsize]);
			stack_y.store(y + 1);
		}
		loop_y.end_block();

		stack_i.store(i + 1);
	}
	loop.end_block();
}

void DrawTriangleCodegen::DrawPartialBlocks(bool isSimpleShade)
{
	stack_i.store(SSAInt(0));
	SSAForBlock loop;
	SSAInt i = stack_i.load();
	loop.loop_block(i < numBlocks, 0);
	{
		SSAInt blockX = SSAShort(partialBlocks[i][0].load(true).v).zext_int();
		SSAInt blockY = SSAShort(partialBlocks[i][1].load(true).v).zext_int();
		SSAInt mask0 = partialBlocks[i][2].load(true);
		SSAInt mask1 = partialBlocks[i][3].load(true);

		SSAUBytePtr dest = destOrg[(blockX + blockY * pitch) * pixelsize];

		SSAStepVariables blockPosY;
		blockPosY.W = start.W + gradientX.W * (blockX - startX) + gradientY.W * (blockY - startY);
		for (int j = 0; j < TriVertex::NumVarying; j++)
			blockPosY.Varying[j] = start.Varying[j] + gradientX.Varying[j] * (blockX - startX) + gradientY.Varying[j] * (blockY - startY);

		for (int maskNum = 0; maskNum < 2; maskNum++)
		{
			SSAInt mask = (maskNum == 0) ? mask0 : mask1;

			for (int y = 0; y < 4; y++)
			{
				SSAStepVariables blockPosX = blockPosY;

				SSAFloat rcpW = SSAFloat((float)0x01000000) / blockPosX.W;
				SSAInt varyingPos[TriVertex::NumVarying];
				for (int j = 0; j < TriVertex::NumVarying; j++)
					varyingPos[j] = SSAInt(blockPosX.Varying[j] * rcpW, false);

				SSAInt lightpos = FRACUNIT - SSAInt(SSAFloat::clamp(shade - SSAFloat::MIN(SSAFloat(24.0f / 32.0f), globVis * blockPosX.W), SSAFloat(0.0f), SSAFloat(31.0f / 32.0f)) * (float)FRACUNIT, true);

				blockPosX.W = blockPosX.W + gradientX.W * 8.0f;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosX.Varying[j] = blockPosX.Varying[j] + gradientX.Varying[j] * 8.0f;

				rcpW = SSAFloat((float)0x01000000) / blockPosX.W;
				SSAInt varyingStep[TriVertex::NumVarying];
				for (int j = 0; j < TriVertex::NumVarying; j++)
				{
					SSAInt nextPos = SSAInt(blockPosX.Varying[j] * rcpW, false);
					varyingStep[j] = (nextPos - varyingPos[j]) / 8;
				}

				SSAInt lightnext = FRACUNIT - SSAInt(SSAFloat::clamp(shade - SSAFloat::MIN(SSAFloat(24.0f / 32.0f), globVis * blockPosX.W), SSAFloat(0.0f), SSAFloat(31.0f / 32.0f)) * (float)FRACUNIT, true);
				SSAInt lightstep = (lightnext - lightpos) / 8;

				for (int x = 0; x < 8; x++)
				{
					SSABool covered = !((mask & (1 << (31 - y * 8 - x))) == SSAInt(0));
					SSAIfBlock branch;
					branch.if_block(covered);
					{
						if (truecolor)
						{
							currentlight = is_fixed_light.select(light, lightpos >> 8);

							SSAUBytePtr destptr = dest[x * 4];
							destptr.store_vec4ub(ProcessPixel32(destptr.load_vec4ub(false), varyingPos, isSimpleShade));
						}
						else
						{
							currentlight = is_fixed_light.select(light, lightpos >> 8);
							SSAInt colormapindex = SSAInt::MIN((256 - currentlight) * 32 / 256, SSAInt(31));
							currentcolormap = Colormaps[colormapindex << 8];

							SSAUBytePtr destptr = dest[x];
							destptr.store(ProcessPixel8(destptr.load(false).zext_int(), varyingPos).trunc_ubyte());
						}
					}
					branch.end_block();

					for (int j = 0; j < TriVertex::NumVarying; j++)
						varyingPos[j] = varyingPos[j] + varyingStep[j];
					lightpos = lightpos + lightstep;
				}

				blockPosY.W = blockPosY.W + gradientY.W;
				for (int j = 0; j < TriVertex::NumVarying; j++)
					blockPosY.Varying[j] = blockPosY.Varying[j] + gradientY.Varying[j];

				dest = dest[pitch * pixelsize];
			}
		}

		stack_i.store(i + 1);
	}
	loop.end_block();
}

SSAVec4i DrawTriangleCodegen::TranslateSample32(SSAInt *varying)
{
	SSAInt ufrac = varying[0] << 8;
	SSAInt vfrac = varying[1] << 8;

	SSAInt upos = ((ufrac >> 16) * textureWidth) >> 16;
	SSAInt vpos = ((vfrac >> 16) * textureHeight) >> 16;
	SSAInt uvoffset = upos * textureHeight + vpos;

	if (colorfill)
		return translation[color * 4].load_vec4ub(true);
	else
		return translation[texturePixels[uvoffset].load(true).zext_int() * 4].load_vec4ub(true);
}

SSAInt DrawTriangleCodegen::TranslateSample8(SSAInt *varying)
{
	SSAInt ufrac = varying[0] << 8;
	SSAInt vfrac = varying[1] << 8;

	SSAInt upos = ((ufrac >> 16) * textureWidth) >> 16;
	SSAInt vpos = ((vfrac >> 16) * textureHeight) >> 16;
	SSAInt uvoffset = upos * textureHeight + vpos;

	if (colorfill)
		return translation[color].load(true).zext_int();
	else
		return translation[texturePixels[uvoffset].load(true).zext_int()].load(true).zext_int();
}

SSAVec4i DrawTriangleCodegen::Sample32(SSAInt *varying)
{
	if (colorfill)
		return SSAVec4i::unpack(color);

	SSAInt ufrac = varying[0] << 8;
	SSAInt vfrac = varying[1] << 8;

	SSAVec4i nearest;
	SSAVec4i linear;

	{
		SSAInt upos = ((ufrac >> 16) * textureWidth) >> 16;
		SSAInt vpos = ((vfrac >> 16) * textureHeight) >> 16;
		SSAInt uvoffset = upos * textureHeight + vpos;

		nearest = texturePixels[uvoffset * 4].load_vec4ub(true);
	}

	return nearest;

	/*
	{
		SSAInt uone = (SSAInt(0x01000000) / textureWidth) << 8;
		SSAInt vone = (SSAInt(0x01000000) / textureHeight) << 8;

		ufrac = ufrac - (uone >> 1);
		vfrac = vfrac - (vone >> 1);

		SSAInt frac_x0 = (ufrac >> FRACBITS) * textureWidth;
		SSAInt frac_x1 = ((ufrac + uone) >> FRACBITS) * textureWidth;
		SSAInt frac_y0 = (vfrac >> FRACBITS) * textureHeight;
		SSAInt frac_y1 = ((vfrac + vone) >> FRACBITS) * textureHeight;

		SSAInt x0 = frac_x0 >> FRACBITS;
		SSAInt x1 = frac_x1 >> FRACBITS;
		SSAInt y0 = frac_y0 >> FRACBITS;
		SSAInt y1 = frac_y1 >> FRACBITS;

		SSAVec4i p00 = texturePixels[(x0 * textureHeight + y0) * 4].load_vec4ub(true);
		SSAVec4i p01 = texturePixels[(x0 * textureHeight + y1) * 4].load_vec4ub(true);
		SSAVec4i p10 = texturePixels[(x1 * textureHeight + y0) * 4].load_vec4ub(true);
		SSAVec4i p11 = texturePixels[(x1 * textureHeight + y1) * 4].load_vec4ub(true);

		SSAInt inv_b = (frac_x1 >> (FRACBITS - 4)) & 15;
		SSAInt inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
		SSAInt a = 16 - inv_a;
		SSAInt b = 16 - inv_b;

		linear = (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
	}

	//	// Min filter = linear, Mag filter = nearest:
	//	AffineLinear = (gradVaryingX[0] / AffineW) > SSAFloat(1.0f) || (gradVaryingX[0] / AffineW) < SSAFloat(-1.0f);

	return AffineLinear.select(linear, nearest);
	*/
}

SSAInt DrawTriangleCodegen::Sample8(SSAInt *varying)
{
	SSAInt ufrac = varying[0] << 8;
	SSAInt vfrac = varying[1] << 8;

	SSAInt upos = ((ufrac >> 16) * textureWidth) >> 16;
	SSAInt vpos = ((vfrac >> 16) * textureHeight) >> 16;
	SSAInt uvoffset = upos * textureHeight + vpos;

	if (colorfill)
		return color;
	else
		return texturePixels[uvoffset].load(true).zext_int();
}

SSAInt DrawTriangleCodegen::Shade8(SSAInt c)
{
	return currentcolormap[c].load(true).zext_int();
}

SSAVec4i DrawTriangleCodegen::Shade32(SSAVec4i fg, SSAInt light, bool isSimpleShade)
{
	if (isSimpleShade)
		return shade_bgra_simple(fg, currentlight);
	else
		return shade_bgra_advanced(fg, currentlight, shade_constants);
}

SSAVec4i DrawTriangleCodegen::ProcessPixel32(SSAVec4i bg, SSAInt *varying, bool isSimpleShade)
{
	SSAVec4i fg;
	SSAVec4i output;

	switch (blendmode)
	{
	default:
	case TriBlendMode::Copy:
		fg = Sample32(varying);
		output = blend_copy(Shade32(fg, currentlight, isSimpleShade));
		break;
	case TriBlendMode::AlphaBlend:
		fg = Sample32(varying);
		output = blend_alpha_blend(Shade32(fg, currentlight, isSimpleShade), bg);
		break;
	case TriBlendMode::AddSolid:
		fg = Sample32(varying);
		output = blend_add(Shade32(fg, currentlight, isSimpleShade), bg, srcalpha, destalpha);
		break;
	case TriBlendMode::Add:
		fg = Sample32(varying);
		output = blend_add(Shade32(fg, currentlight, isSimpleShade), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::Sub:
		fg = Sample32(varying);
		output = blend_sub(Shade32(fg, currentlight, isSimpleShade), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::RevSub:
		fg = Sample32(varying);
		output = blend_revsub(Shade32(fg, currentlight, isSimpleShade), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::Stencil:
		fg = Sample32(varying);
		output = blend_stencil(Shade32(SSAVec4i::unpack(color), currentlight, isSimpleShade), fg[3], bg, srcalpha, destalpha);
		break;
	case TriBlendMode::Shaded:
		output = blend_stencil(Shade32(SSAVec4i::unpack(color), currentlight, isSimpleShade), Sample8(varying), bg, srcalpha, destalpha);
		break;
	case TriBlendMode::TranslateCopy:
		fg = TranslateSample32(varying);
		output = blend_copy(Shade32(fg, currentlight, isSimpleShade));
		break;
	case TriBlendMode::TranslateAlphaBlend:
		fg = TranslateSample32(varying);
		output = blend_alpha_blend(Shade32(fg, currentlight, isSimpleShade), bg);
		break;
	case TriBlendMode::TranslateAdd:
		fg = TranslateSample32(varying);
		output = blend_add(Shade32(fg, currentlight, isSimpleShade), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::TranslateSub:
		fg = TranslateSample32(varying);
		output = blend_sub(Shade32(fg, currentlight, isSimpleShade), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::TranslateRevSub:
		fg = TranslateSample32(varying);
		output = blend_revsub(Shade32(fg, currentlight, isSimpleShade), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::AddSrcColorOneMinusSrcColor:
		fg = Sample32(varying);
		output = blend_add_srccolor_oneminussrccolor(Shade32(fg, currentlight, isSimpleShade), bg);
		break;
	case TriBlendMode::Skycap:
		fg = Sample32(varying);
		output = FadeOut(varying[1], fg);
		break;
	}

	return output;
}

SSAVec4i DrawTriangleCodegen::ToBgra(SSAInt index)
{
	SSAVec4i c = BaseColors[index * 4].load_vec4ub(true);
	c = c.insert(3, 255);
	return c;
}

SSAInt DrawTriangleCodegen::ToPal8(SSAVec4i c)
{
	SSAInt red = SSAInt::clamp(c[0], SSAInt(0), SSAInt(255));
	SSAInt green = SSAInt::clamp(c[1], SSAInt(0), SSAInt(255));
	SSAInt blue = SSAInt::clamp(c[2], SSAInt(0), SSAInt(255));
	return RGB256k[((blue >> 2) * 64 + (green >> 2)) * 64 + (red >> 2)].load(true).zext_int();
}

SSAInt DrawTriangleCodegen::ProcessPixel8(SSAInt bg, SSAInt *varying)
{
	SSAVec4i fg;
	SSAInt alpha, inv_alpha;
	SSAInt output;
	SSAInt palindex;

	switch (blendmode)
	{
	default:
	case TriBlendMode::Copy:
		output = Shade8(Sample8(varying));
		break;
	case TriBlendMode::AlphaBlend:
		palindex = Sample8(varying);
		output = Shade8(palindex);
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::AddSolid:
		palindex = Sample8(varying);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_add(fg, ToBgra(bg), srcalpha, destalpha));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::Add:
		palindex = Sample8(varying);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_add(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::Sub:
		palindex = Sample8(varying);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_sub(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::RevSub:
		palindex = Sample8(varying);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_revsub(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::Stencil:
		output = ToPal8(blend_stencil(ToBgra(Shade8(color)), (Sample8(varying) == SSAInt(0)).select(SSAInt(0), SSAInt(256)), ToBgra(bg), srcalpha, destalpha));
		break;
	case TriBlendMode::Shaded:
		palindex = Sample8(varying);
		output = ToPal8(blend_stencil(ToBgra(Shade8(color)), palindex, ToBgra(bg), srcalpha, destalpha));
		break;
	case TriBlendMode::TranslateCopy:
		palindex = TranslateSample8(varying);
		output = Shade8(palindex);
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::TranslateAlphaBlend:
		palindex = TranslateSample8(varying);
		output = Shade8(palindex);
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::TranslateAdd:
		palindex = TranslateSample8(varying);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_add(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::TranslateSub:
		palindex = TranslateSample8(varying);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_sub(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::TranslateRevSub:
		palindex = TranslateSample8(varying);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_revsub(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::AddSrcColorOneMinusSrcColor:
		palindex = Sample8(varying);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_add_srccolor_oneminussrccolor(fg, ToBgra(bg)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::Skycap:
		fg = ToBgra(Sample8(varying));
		output = ToPal8(FadeOut(varying[1], fg));
		break;
	}

	return output;
}

SSAVec4i DrawTriangleCodegen::FadeOut(SSAInt frac, SSAVec4i fg)
{
	int start_fade = 2; // How fast it should fade out

	SSAInt alpha_top = SSAInt::MAX(SSAInt::MIN(frac.ashr(16 - start_fade), SSAInt(256)), SSAInt(0));
	SSAInt alpha_bottom = SSAInt::MAX(SSAInt::MIN(((2 << 24) - frac).ashr(16 - start_fade), SSAInt(256)), SSAInt(0));
	SSAInt alpha = SSAInt::MIN(alpha_top, alpha_bottom);
	SSAInt inv_alpha = 256 - alpha;

	fg = (fg * alpha + SSAVec4i::unpack(color) * inv_alpha) / 256;
	return fg.insert(3, 255);
}

void DrawTriangleCodegen::CalculateGradients()
{
	gradientX.W = FindGradientX(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	gradientY.W = FindGradientY(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	start.W = v1.w + gradientX.W * (SSAFloat(startX) - v1.x) + gradientY.W * (SSAFloat(startY) - v1.y);
	for (int i = 0; i < TriVertex::NumVarying; i++)
	{
		gradientX.Varying[i] = FindGradientX(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		gradientY.Varying[i] = FindGradientY(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		start.Varying[i] = v1.varying[i] * v1.w + gradientX.Varying[i] * (SSAFloat(startX) - v1.x) + gradientY.Varying[i] * (SSAFloat(startY) - v1.y);
	}

	shade = (64.0f - (SSAFloat(light * 255 / 256) + 12.0f) * 32.0f / 128.0f) / 32.0f;
	globVis = SSAFloat(1706.0f / 32.0f);
}

void DrawTriangleCodegen::LoadArgs(SSAValue args, SSAValue thread_data)
{
	destOrg = args[0][0].load(true);
	pitch = args[0][1].load(true);
	v1 = LoadTriVertex(args[0][2].load(true));
	v2 = LoadTriVertex(args[0][3].load(true));
	v3 = LoadTriVertex(args[0][4].load(true));
	texturePixels = args[0][9].load(true);
	textureWidth = args[0][10].load(true);
	textureHeight = args[0][11].load(true);
	translation = args[0][12].load(true);
	LoadUniforms(args[0][13].load(true));
	if (!truecolor)
	{
		Colormaps = args[0][20].load(true);
		RGB256k = args[0][21].load(true);
		BaseColors = args[0][22].load(true);
	}

	fullSpans = thread_data[0][5].load(true);
	partialBlocks = thread_data[0][6].load(true);
	numSpans = thread_data[0][7].load(true);
	numBlocks = thread_data[0][8].load(true);
	startX = thread_data[0][9].load(true);
	startY = thread_data[0][10].load(true);
}

SSATriVertex DrawTriangleCodegen::LoadTriVertex(SSAValue ptr)
{
	SSATriVertex v;
	v.x = ptr[0][0].load(true);
	v.y = ptr[0][1].load(true);
	v.z = ptr[0][2].load(true);
	v.w = ptr[0][3].load(true);
	for (int i = 0; i < TriVertex::NumVarying; i++)
		v.varying[i] = ptr[0][4 + i].load(true);
	return v;
}

void DrawTriangleCodegen::LoadUniforms(SSAValue uniforms)
{
	light = uniforms[0][0].load(true);
	color = uniforms[0][2].load(true);
	srcalpha = uniforms[0][3].load(true);
	destalpha = uniforms[0][4].load(true);

	SSAShort light_alpha = uniforms[0][5].load(true);
	SSAShort light_red = uniforms[0][6].load(true);
	SSAShort light_green = uniforms[0][7].load(true);
	SSAShort light_blue = uniforms[0][8].load(true);
	SSAShort fade_alpha = uniforms[0][9].load(true);
	SSAShort fade_red = uniforms[0][10].load(true);
	SSAShort fade_green = uniforms[0][11].load(true);
	SSAShort fade_blue = uniforms[0][12].load(true);
	SSAShort desaturate = uniforms[0][13].load(true);
	SSAInt flags = uniforms[0][14].load(true);
	shade_constants.light = SSAVec4i(light_blue.zext_int(), light_green.zext_int(), light_red.zext_int(), light_alpha.zext_int());
	shade_constants.fade = SSAVec4i(fade_blue.zext_int(), fade_green.zext_int(), fade_red.zext_int(), fade_alpha.zext_int());
	shade_constants.desaturate = desaturate.zext_int();

	is_simple_shade = (flags & TriUniforms::simple_shade) == SSAInt(TriUniforms::simple_shade);
	is_nearest_filter = (flags & TriUniforms::nearest_filter) == SSAInt(TriUniforms::nearest_filter);
	is_fixed_light = (flags & TriUniforms::fixed_light) == SSAInt(TriUniforms::fixed_light);
}

SSAFloat DrawTriangleCodegen::FindGradientX(SSAFloat x0, SSAFloat y0, SSAFloat x1, SSAFloat y1, SSAFloat x2, SSAFloat y2, SSAFloat c0, SSAFloat c1, SSAFloat c2)
{
	SSAFloat top = (c1 - c2) * (y0 - y2) - (c0 - c2) * (y1 - y2);
	SSAFloat bottom = (x1 - x2) * (y0 - y2) - (x0 - x2) * (y1 - y2);
	return top / bottom;
}

SSAFloat DrawTriangleCodegen::FindGradientY(SSAFloat x0, SSAFloat y0, SSAFloat x1, SSAFloat y1, SSAFloat x2, SSAFloat y2, SSAFloat c0, SSAFloat c1, SSAFloat c2)
{
	SSAFloat top = (c1 - c2) * (x0 - x2) - (c0 - c2) * (x1 - x2);
	SSAFloat bottom = (x0 - x2) * (y1 - y2) - (x1 - x2) * (y0 - y2);
	return top / bottom;
}
