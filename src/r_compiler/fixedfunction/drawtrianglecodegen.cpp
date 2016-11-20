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

#include "i_system.h"
#include "r_compiler/llvm_include.h"
#include "r_compiler/fixedfunction/drawtrianglecodegen.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"

void DrawTriangleCodegen::Generate(TriDrawVariant variant, TriBlendMode blendmode, bool truecolor, SSAValue args, SSAValue thread_data)
{
	this->variant = variant;
	this->blendmode = blendmode;
	this->truecolor = truecolor;
	LoadArgs(args, thread_data);
	Setup();
	LoopBlockY();
}

SSAInt DrawTriangleCodegen::FloatTo28_4(SSAFloat v)
{
	// SSAInt(SSAFloat::round(16.0f * v), false);
	SSAInt a = SSAInt(v * 32.0f, false);
	return (a + (a.ashr(31) | SSAInt(1))).ashr(1);
}

void DrawTriangleCodegen::Setup()
{
	int pixelsize = truecolor ? 4 : 1;

	// 28.4 fixed-point coordinates
	Y1 = FloatTo28_4(v1.y);
	Y2 = FloatTo28_4(v2.y);
	Y3 = FloatTo28_4(v3.y);

	X1 = FloatTo28_4(v1.x);
	X2 = FloatTo28_4(v2.x);
	X3 = FloatTo28_4(v3.x);

	// Deltas
	DX12 = X1 - X2;
	DX23 = X2 - X3;
	DX31 = X3 - X1;

	DY12 = Y1 - Y2;
	DY23 = Y2 - Y3;
	DY31 = Y3 - Y1;

	// Fixed-point deltas
	FDX12 = DX12 << 4;
	FDX23 = DX23 << 4;
	FDX31 = DX31 << 4;

	FDY12 = DY12 << 4;
	FDY23 = DY23 << 4;
	FDY31 = DY31 << 4;

	// Bounding rectangle
	minx = SSAInt::MAX((SSAInt::MIN(SSAInt::MIN(X1, X2), X3) + 0xF).ashr(4), clipleft);
	maxx = SSAInt::MIN((SSAInt::MAX(SSAInt::MAX(X1, X2), X3) + 0xF).ashr(4), clipright - 1);
	miny = SSAInt::MAX((SSAInt::MIN(SSAInt::MIN(Y1, Y2), Y3) + 0xF).ashr(4), cliptop);
	maxy = SSAInt::MIN((SSAInt::MAX(SSAInt::MAX(Y1, Y2), Y3) + 0xF).ashr(4), clipbottom - 1);

	SSAIfBlock if0;
	if0.if_block(minx >= maxx || miny >= maxy);
	if0.end_retvoid();

	// Start in corner of 8x8 block
	minx = minx & ~(q - 1);
	miny = miny & ~(q - 1);

	dest = dest[miny * pitch * pixelsize];
	subsectorGBuffer = subsectorGBuffer[miny * pitch];

	// Half-edge constants
	C1 = DY12 * X1 - DX12 * Y1;
	C2 = DY23 * X2 - DX23 * Y2;
	C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	SSAIfBlock if1;
	if1.if_block(DY12 < SSAInt(0) || (DY12 == SSAInt(0) && DX12 > SSAInt(0)));
		stack_C1.store(C1 + 1);
	if1.else_block();
		stack_C1.store(C1);
	if1.end_block();
	C1 = stack_C1.load();
	SSAIfBlock if2;
	if2.if_block(DY23 < SSAInt(0) || (DY23 == SSAInt(0) && DX23 > SSAInt(0)));
		stack_C2.store(C2 + 1);
	if2.else_block();
		stack_C2.store(C2);
	if2.end_block();
	C2 = stack_C2.load();
	SSAIfBlock if3;
	if3.if_block(DY31 < SSAInt(0) || (DY31 == SSAInt(0) && DX31 > SSAInt(0)));
		stack_C3.store(C3 + 1);
	if3.else_block();
		stack_C3.store(C3);
	if3.end_block();
	C3 = stack_C3.load();

	// Gradients
	v1.x = SSAFloat(X1) * 0.0625f;
	v2.x = SSAFloat(X2) * 0.0625f;
	v3.x = SSAFloat(X3) * 0.0625f;
	v1.y = SSAFloat(Y1) * 0.0625f;
	v2.y = SSAFloat(Y2) * 0.0625f;
	v3.y = SSAFloat(Y3) * 0.0625f;
	gradWX = gradx(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	gradWY = grady(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.w, v2.w, v3.w);
	startW = v1.w + gradWX * (SSAFloat(minx) - v1.x) + gradWY * (SSAFloat(miny) - v1.y);
	for (int i = 0; i < TriVertex::NumVarying; i++)
	{
		gradVaryingX[i] = gradx(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		gradVaryingY[i] = grady(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		startVarying[i] = v1.varying[i] * v1.w + gradVaryingX[i] * (SSAFloat(minx) - v1.x) + gradVaryingY[i] * (SSAFloat(miny) - v1.y);
	}
}

SSAFloat DrawTriangleCodegen::gradx(SSAFloat x0, SSAFloat y0, SSAFloat x1, SSAFloat y1, SSAFloat x2, SSAFloat y2, SSAFloat c0, SSAFloat c1, SSAFloat c2)
{
	SSAFloat top = (c1 - c2) * (y0 - y2) - (c0 - c2) * (y1 - y2);
	SSAFloat bottom = (x1 - x2) * (y0 - y2) - (x0 - x2) * (y1 - y2);
	return top / bottom;
}

SSAFloat DrawTriangleCodegen::grady(SSAFloat x0, SSAFloat y0, SSAFloat x1, SSAFloat y1, SSAFloat x2, SSAFloat y2, SSAFloat c0, SSAFloat c1, SSAFloat c2)
{
	SSAFloat top = (c1 - c2) * (x0 - x2) - (c0 - c2) * (x1 - x2);
	SSAFloat bottom = (x0 - x2) * (y1 - y2) - (x1 - x2) * (y0 - y2);
	return top / bottom;
}

void DrawTriangleCodegen::LoopBlockY()
{
	int pixelsize = truecolor ? 4 : 1;

	stack_y.store(miny);
	stack_dest.store(dest);
	stack_subsectorGBuffer.store(subsectorGBuffer);

	SSAForBlock loop;
	y = stack_y.load();
	dest = stack_dest.load();
	subsectorGBuffer = stack_subsectorGBuffer.load();
	loop.loop_block(y < maxy, 0);
	{
		SSAIfBlock branch;
		branch.if_block((y / q) % thread.num_cores == thread.core);
		{
			LoopBlockX();
		}
		branch.end_block();

		stack_dest.store(dest[q * pitch * pixelsize]);
		stack_subsectorGBuffer.store(subsectorGBuffer[q * pitch]);
		stack_y.store(y + q);
	}
	loop.end_block();
}

void DrawTriangleCodegen::LoopBlockX()
{
	stack_x.store(minx);

	SSAForBlock loop;
	x = stack_x.load();
	loop.loop_block(x < maxx, 0);
	{
		// Corners of block
		x0 = x << 4;
		x1 = (x + q - 1) << 4;
		y0 = y << 4;
		y1 = (y + q - 1) << 4;

		// Evaluate half-space functions
		SSABool a00 = C1 + DX12 * y0 - DY12 * x0 > SSAInt(0);
		SSABool a10 = C1 + DX12 * y0 - DY12 * x1 > SSAInt(0);
		SSABool a01 = C1 + DX12 * y1 - DY12 * x0 > SSAInt(0);
		SSABool a11 = C1 + DX12 * y1 - DY12 * x1 > SSAInt(0);
		
		SSAInt a = (a00.zext_int() << 0) | (a10.zext_int() << 1) | (a01.zext_int() << 2) | (a11.zext_int() << 3);

		SSABool b00 = C2 + DX23 * y0 - DY23 * x0 > SSAInt(0);
		SSABool b10 = C2 + DX23 * y0 - DY23 * x1 > SSAInt(0);
		SSABool b01 = C2 + DX23 * y1 - DY23 * x0 > SSAInt(0);
		SSABool b11 = C2 + DX23 * y1 - DY23 * x1 > SSAInt(0);
		SSAInt b = (b00.zext_int() << 0) | (b10.zext_int() << 1) | (b01.zext_int() << 2) | (b11.zext_int() << 3);

		SSABool c00 = C3 + DX31 * y0 - DY31 * x0 > SSAInt(0);
		SSABool c10 = C3 + DX31 * y0 - DY31 * x1 > SSAInt(0);
		SSABool c01 = C3 + DX31 * y1 - DY31 * x0 > SSAInt(0);
		SSABool c11 = C3 + DX31 * y1 - DY31 * x1 > SSAInt(0);
		SSAInt c = (c00.zext_int() << 0) | (c10.zext_int() << 1) | (c01.zext_int() << 2) | (c11.zext_int() << 3);
		
		// Skip block when outside an edge
		SSAIfBlock branch;
		branch.if_block(!(a == SSAInt(0) || b == SSAInt(0) || c == SSAInt(0)));

		// Check if block needs clipping
		SSABool clipneeded = x < clipleft || (x + q) > clipright || y < cliptop || (y + q) > clipbottom;

		// Calculate varying variables for affine block
		SSAFloat offx0 = SSAFloat(x - minx);
		SSAFloat offy0 = SSAFloat(y - miny);
		SSAFloat offx1 = offx0 + SSAFloat(q);
		SSAFloat offy1 = offy0 + SSAFloat(q);
		SSAFloat rcpWTL = 1.0f / (startW + offx0 * gradWX + offy0 * gradWY);
		SSAFloat rcpWTR = 1.0f / (startW + offx1 * gradWX + offy0 * gradWY);
		SSAFloat rcpWBL = 1.0f / (startW + offx0 * gradWX + offy1 * gradWY);
		SSAFloat rcpWBR = 1.0f / (startW + offx1 * gradWX + offy1 * gradWY);
		for (int i = 0; i < TriVertex::NumVarying; i++)
		{
			SSAFloat varyingTL = (startVarying[i] + offx0 * gradVaryingX[i] + offy0 * gradVaryingY[i]) * rcpWTL;
			SSAFloat varyingTR = (startVarying[i] + offx1 * gradVaryingX[i] + offy0 * gradVaryingY[i]) * rcpWTR;
			SSAFloat varyingBL = (startVarying[i] + offx0 * gradVaryingX[i] + offy1 * gradVaryingY[i]) * rcpWBL;
			SSAFloat varyingBR = (startVarying[i] + offx1 * gradVaryingX[i] + offy1 * gradVaryingY[i]) * rcpWBR;

			SSAFloat pos = varyingTL;
			SSAFloat stepPos = (varyingBL - varyingTL) * (1.0f / q);
			SSAFloat startStepX = (varyingTR - varyingTL) * (1.0f / q);
			SSAFloat incrStepX = (varyingBR - varyingBL) * (1.0f / q) - startStepX;

			varyingPos[i] = SSAInt(pos * SSAFloat((float)0x01000000), false);
			varyingStepPos[i] = SSAInt(stepPos * SSAFloat((float)0x01000000), false);
			varyingStartStepX[i] = SSAInt(startStepX * SSAFloat((float)0x01000000), false);
			varyingIncrStepX[i] = SSAInt(incrStepX * SSAFloat((float)0x01000000), false);
		}

		SSAFloat globVis = SSAFloat(1706.0f);
		SSAFloat vis = globVis / rcpWTL;
		SSAFloat shade = 64.0f - (SSAFloat(light * 255 / 256) + 12.0f) * 32.0f / 128.0f;
		SSAFloat lightscale = SSAFloat::clamp((shade - SSAFloat::MIN(SSAFloat(24.0f), vis)) / 32.0f, SSAFloat(0.0f), SSAFloat(31.0f / 32.0f));
		SSAInt diminishedlight = SSAInt(SSAFloat::clamp((1.0f - lightscale) * 256.0f + 0.5f, SSAFloat(0.0f), SSAFloat(256.0f)), false);
		currentlight = is_fixed_light.select(light, diminishedlight);

		SetStencilBlock(x / 8 + y / 8 * stencilPitch);

		SSABool covered = a == SSAInt(0xF) && b == SSAInt(0xF) && c == SSAInt(0xF) && !clipneeded;
		if (variant != TriDrawVariant::DrawSubsector && variant != TriDrawVariant::FillSubsector && variant != TriDrawVariant::FuzzSubsector)
		{
			covered = covered && StencilIsSingleValue();
		}

		// Accept whole block when totally covered
		SSAIfBlock branch_covered;
		branch_covered.if_block(covered);
		{
			LoopFullBlock();
		}
		branch_covered.else_block();
		{
			LoopPartialBlock();
		}
		branch_covered.end_block();

		branch.end_block();

		stack_x.store(x + q);
	}
	loop.end_block();
}

void DrawTriangleCodegen::LoopFullBlock()
{
	SSAIfBlock branch_stenciltest;
	if (variant != TriDrawVariant::DrawSubsector && variant != TriDrawVariant::FillSubsector && variant != TriDrawVariant::FuzzSubsector)
	{
		branch_stenciltest.if_block(StencilGetSingle() == stencilTestValue);
	}

	if (variant == TriDrawVariant::Stencil)
	{
		StencilClear(stencilWriteValue);
	}
	else
	{
		int pixelsize = truecolor ? 4 : 1;

		for (int iy = 0; iy < q; iy++)
		{
			SSAUBytePtr buffer = dest[(x + iy * pitch) * pixelsize];
			SSAIntPtr subsectorbuffer = subsectorGBuffer[x + iy * pitch];

			SSAInt varying[TriVertex::NumVarying];
			SSAInt varyingStep[TriVertex::NumVarying];
			for (int i = 0; i < TriVertex::NumVarying; i++)
			{
				varying[i] = (varyingPos[i] + varyingStepPos[i] * iy) << 8;
				varyingStep[i] = (varyingStartStepX[i] + varyingIncrStepX[i] * iy) << 8;
			}

			for (int ix = 0; ix < q; ix += 4)
			{
				SSAUBytePtr buf = buffer[ix * pixelsize];
				if (truecolor)
				{
					SSAVec16ub pixels16 = buf.load_unaligned_vec16ub(false);
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
						if (variant == TriDrawVariant::DrawSubsector || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
						{
							SSABool subsectorTest = subsectorbuffer[ix].load(true) >= subsectorDepth;
							pixels[sse] = subsectorTest.select(ProcessPixel32(pixels[sse], varying), pixels[sse]);
						}
						else
						{
							pixels[sse] = ProcessPixel32(pixels[sse], varying);
						}

						for (int i = 0; i < TriVertex::NumVarying; i++)
							varying[i] = varying[i] + varyingStep[i];
					}

					buf.store_unaligned_vec16ub(SSAVec16ub(SSAVec8s(pixels[0], pixels[1]), SSAVec8s(pixels[2], pixels[3])));
				}
				else
				{
					SSAVec4i pixels = buf.load_vec4ub(false);

					for (int sse = 0; sse < 4; sse++)
					{
						if (variant == TriDrawVariant::DrawSubsector || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
						{
							SSABool subsectorTest = subsectorbuffer[ix].load(true) >= subsectorDepth;
							pixels.insert(sse, subsectorTest.select(ProcessPixel8(pixels[sse], varying), pixels[sse]));
						}
						else
						{
							pixels.insert(sse, ProcessPixel8(pixels[sse], varying));
						}

						for (int i = 0; i < TriVertex::NumVarying; i++)
							varying[i] = varying[i] + varyingStep[i];
					}

					buf.store_vec4ub(pixels);
				}

				if (variant != TriDrawVariant::DrawSubsector && variant != TriDrawVariant::FillSubsector && variant != TriDrawVariant::FuzzSubsector)
					subsectorbuffer[ix].store_unaligned_vec4i(SSAVec4i(subsectorDepth));
			}
		}
	}

	if (variant != TriDrawVariant::DrawSubsector && variant != TriDrawVariant::FillSubsector && variant != TriDrawVariant::FuzzSubsector)
	{
		branch_stenciltest.end_block();
	}
}

void DrawTriangleCodegen::LoopPartialBlock()
{
	int pixelsize = truecolor ? 4 : 1;

	stack_CY1.store(C1 + DX12 * y0 - DY12 * x0);
	stack_CY2.store(C2 + DX23 * y0 - DY23 * x0);
	stack_CY3.store(C3 + DX31 * y0 - DY31 * x0);
	stack_iy.store(SSAInt(0));
	stack_buffer.store(dest[x * pixelsize]);
	stack_subsectorbuffer.store(subsectorGBuffer[x]);

	SSAForBlock loopy;
	SSAInt iy = stack_iy.load();
	SSAUBytePtr buffer = stack_buffer.load();
	SSAIntPtr subsectorbuffer = stack_subsectorbuffer.load();
	SSAInt CY1 = stack_CY1.load();
	SSAInt CY2 = stack_CY2.load();
	SSAInt CY3 = stack_CY3.load();
	loopy.loop_block(iy < SSAInt(q), q);
	{
		SSAInt varyingStep[TriVertex::NumVarying];
		for (int i = 0; i < TriVertex::NumVarying; i++)
		{
			stack_varying[i].store((varyingPos[i] + varyingStepPos[i] * iy) << 8);
			varyingStep[i] = (varyingStartStepX[i] + varyingIncrStepX[i] * iy) << 8;
		}

		stack_CX1.store(CY1);
		stack_CX2.store(CY2);
		stack_CX3.store(CY3);
		stack_ix.store(SSAInt(0));

		SSAForBlock loopx;
		SSAInt ix = stack_ix.load();
		SSAInt CX1 = stack_CX1.load();
		SSAInt CX2 = stack_CX2.load();
		SSAInt CX3 = stack_CX3.load();
		SSAInt varying[TriVertex::NumVarying];
		for (int i = 0; i < TriVertex::NumVarying; i++)
			varying[i] = stack_varying[i].load();
		loopx.loop_block(ix < SSAInt(q), q);
		{
			SSABool visible = (ix + x >= clipleft) && (ix + x < clipright) && (iy + y >= cliptop) && (iy + y < clipbottom);
			SSABool covered = CX1 > SSAInt(0) && CX2 > SSAInt(0) && CX3 > SSAInt(0) && visible;

			if (variant == TriDrawVariant::DrawSubsector || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
			{
				covered = covered && subsectorbuffer[ix].load(true) >= subsectorDepth;
			}
			else
			{
				covered = covered && StencilGet(ix, iy) == stencilTestValue;
			}

			SSAIfBlock branch;
			branch.if_block(covered);
			{
				if (variant == TriDrawVariant::Stencil)
				{
					StencilSet(ix, iy, stencilWriteValue);
				}
				else
				{
					SSAUBytePtr buf = buffer[ix * pixelsize];

					if (truecolor)
					{
						SSAVec4i bg = buf.load_vec4ub(false);
						buf.store_vec4ub(ProcessPixel32(bg, varying));
					}
					else
					{
						SSAUByte bg = buf.load(false);
						buf.store(ProcessPixel8(bg.zext_int(), varying).trunc_ubyte());
					}

					if (variant != TriDrawVariant::DrawSubsector && variant != TriDrawVariant::FillSubsector && variant != TriDrawVariant::FuzzSubsector)
						subsectorbuffer[ix].store(subsectorDepth);
				}
			}
			branch.end_block();

			for (int i = 0; i < TriVertex::NumVarying; i++)
				stack_varying[i].store(varying[i] + varyingStep[i]);

			stack_CX1.store(CX1 - FDY12);
			stack_CX2.store(CX2 - FDY23);
			stack_CX3.store(CX3 - FDY31);
			stack_ix.store(ix + 1);
		}
		loopx.end_block();

		stack_CY1.store(CY1 + FDX12);
		stack_CY2.store(CY2 + FDX23);
		stack_CY3.store(CY3 + FDX31);
		stack_buffer.store(buffer[pitch * pixelsize]);
		stack_subsectorbuffer.store(subsectorbuffer[pitch]);
		stack_iy.store(iy + 1);
	}
	loopy.end_block();
}

SSAVec4i DrawTriangleCodegen::TranslateSample(SSAInt uvoffset)
{
	if (variant == TriDrawVariant::FillNormal || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
		return translation[color * 4].load_vec4ub(true);
	else
		return translation[texturePixels[uvoffset].load(true).zext_int() * 4].load_vec4ub(true);
}

SSAVec4i DrawTriangleCodegen::Sample(SSAInt uvoffset)
{
	if (variant == TriDrawVariant::FillNormal || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
		return SSAVec4i::unpack(color);
	else
		return texturePixels[uvoffset * 4].load_vec4ub(true);
}

SSAVec4i DrawTriangleCodegen::ProcessPixel32(SSAVec4i bg, SSAInt *varying)
{
	SSAInt ufrac = varying[0];
	SSAInt vfrac = varying[1];

	SSAInt upos = ((ufrac >> 16) * textureWidth) >> 16;
	SSAInt vpos = ((vfrac >> 16) * textureHeight) >> 16;
	SSAInt uvoffset = upos * textureHeight + vpos;

	SSAVec4i fg;
	SSAInt alpha, inv_alpha;
	SSAVec4i output;

	switch (blendmode)
	{
	default:
	case TriBlendMode::Copy:
		fg = Sample(uvoffset);
		output = blend_copy(shade_bgra_simple(fg, currentlight)); break;
	case TriBlendMode::AlphaBlend:
		fg = Sample(uvoffset);
		output = blend_alpha_blend(shade_bgra_simple(fg, currentlight), bg); break;
	case TriBlendMode::AddSolid:
		fg = Sample(uvoffset);
		output = blend_add(shade_bgra_simple(fg, currentlight), bg, srcalpha, destalpha); break;
	case TriBlendMode::Add:
		fg = Sample(uvoffset);
		output = blend_add(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha)); break;
	case TriBlendMode::Sub:
		fg = Sample(uvoffset);
		output = blend_sub(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha)); break;
	case TriBlendMode::RevSub:
		fg = Sample(uvoffset);
		output = blend_revsub(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha)); break;
	case TriBlendMode::Shaded:
		fg = Sample(uvoffset);
		alpha = fg[0];
		alpha = alpha + (alpha >> 7); // 255 -> 256
		inv_alpha = 256 - alpha;
		output = blend_add(shade_bgra_simple(SSAVec4i::unpack(color), currentlight), bg, alpha, inv_alpha);
		break;
	case TriBlendMode::TranslateCopy:
		fg = TranslateSample(uvoffset);
		output = blend_copy(shade_bgra_simple(fg, currentlight));
		break;
	case TriBlendMode::TranslateAlphaBlend:
		fg = TranslateSample(uvoffset);
		output = blend_alpha_blend(shade_bgra_simple(fg, currentlight), bg); break;
		break;
	case TriBlendMode::TranslateAdd:
		fg = TranslateSample(uvoffset);
		output = blend_add(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::TranslateSub:
		fg = TranslateSample(uvoffset);
		output = blend_sub(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::TranslateRevSub:
		fg = TranslateSample(uvoffset);
		output = blend_revsub(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	}

	return output;
}

SSAInt DrawTriangleCodegen::ProcessPixel8(SSAInt bg, SSAInt *varying)
{
	SSAInt ufrac = varying[0];
	SSAInt vfrac = varying[1];

	SSAInt upos = ((ufrac >> 16) * textureWidth) >> 16;
	SSAInt vpos = ((vfrac >> 16) * textureHeight) >> 16;
	SSAInt uvoffset = upos * textureHeight + vpos;

	if (variant == TriDrawVariant::FillNormal || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
	{
		return color;
	}
	else
	{
		SSAUByte fg = texturePixels[uvoffset].load(true);
		return fg.zext_int();
	}
}

void DrawTriangleCodegen::SetStencilBlock(SSAInt block)
{
	StencilBlock = stencilValues[block * 64];
	StencilBlockMask = stencilMasks[block];
}

void DrawTriangleCodegen::StencilSet(SSAInt x, SSAInt y, SSAUByte value)
{
	SSAInt mask = StencilBlockMask.load(false);

	SSAIfBlock branchNeedsUpdate;
	branchNeedsUpdate.if_block(!(mask == (SSAInt(0xffffff00) | value.zext_int())));

	SSAIfBlock branchFirstSet;
	branchFirstSet.if_block((mask & SSAInt(0xffffff00)) == SSAInt(0xffffff00));
	{
		SSAUByte val0 = mask.trunc_ubyte();
		for (int i = 0; i < 8 * 8; i++)
			StencilBlock[i].store(val0);
		StencilBlockMask.store(SSAInt(0));
	}
	branchFirstSet.end_block();

	StencilBlock[x + y * 8].store(value);

	branchNeedsUpdate.end_block();
}

SSAUByte DrawTriangleCodegen::StencilGet(SSAInt x, SSAInt y)
{
	return StencilIsSingleValue().select(StencilBlockMask.load(false).trunc_ubyte(), StencilBlock[x + y * 8].load(false));
}

SSAUByte DrawTriangleCodegen::StencilGetSingle()
{
	return StencilBlockMask.load(false).trunc_ubyte();
}

void DrawTriangleCodegen::StencilClear(SSAUByte value)
{
	StencilBlockMask.store(SSAInt(0xffffff00) | value.zext_int());
}

SSABool DrawTriangleCodegen::StencilIsSingleValue()
{
	return (StencilBlockMask.load(false) & SSAInt(0xffffff00)) == SSAInt(0xffffff00);
}

void DrawTriangleCodegen::LoadArgs(SSAValue args, SSAValue thread_data)
{
	dest = args[0][0].load(true);
	pitch = args[0][1].load(true);
	v1 = LoadTriVertex(args[0][2].load(true));
	v2 = LoadTriVertex(args[0][3].load(true));
	v3 = LoadTriVertex(args[0][4].load(true));
	clipleft = SSAInt(0);// args[0][5].load(true);
	clipright = args[0][6].load(true);
	cliptop = SSAInt(0);// args[0][7].load(true);
	clipbottom = args[0][8].load(true);
	texturePixels = args[0][9].load(true);
	textureWidth = args[0][10].load(true);
	textureHeight = args[0][11].load(true);
	translation = args[0][12].load(true);
	LoadUniforms(args[0][13].load(true));
	stencilValues = args[0][14].load(true);
	stencilMasks = args[0][15].load(true);
	stencilPitch = args[0][16].load(true);
	stencilTestValue = args[0][17].load(true);
	stencilWriteValue = args[0][18].load(true);
	subsectorGBuffer = args[0][19].load(true);

	thread.core = thread_data[0][0].load(true);
	thread.num_cores = thread_data[0][1].load(true);
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
	subsectorDepth = uniforms[0][1].load(true);
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
