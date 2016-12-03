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
	minx = SSAInt::MAX((SSAInt::MIN(SSAInt::MIN(X1, X2), X3) + 0xF).ashr(4), SSAInt(0));
	maxx = SSAInt::MIN((SSAInt::MAX(SSAInt::MAX(X1, X2), X3) + 0xF).ashr(4), clipright - 1);
	miny = SSAInt::MAX((SSAInt::MIN(SSAInt::MIN(Y1, Y2), Y3) + 0xF).ashr(4), SSAInt(0));
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
	stack_posy_w.store(v1.w + gradWX * (SSAFloat(minx) - v1.x) + gradWY * (SSAFloat(miny) - v1.y));
	for (int i = 0; i < TriVertex::NumVarying; i++)
	{
		gradVaryingX[i] = gradx(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		gradVaryingY[i] = grady(v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v1.varying[i] * v1.w, v2.varying[i] * v2.w, v3.varying[i] * v3.w);
		stack_posy_varying[i].store(v1.varying[i] * v1.w + gradVaryingX[i] * (SSAFloat(minx) - v1.x) + gradVaryingY[i] * (SSAFloat(miny) - v1.y));
	}

	gradWX = gradWX * (float)q;
	for (int i = 0; i < TriVertex::NumVarying; i++)
		gradVaryingX[i] = gradVaryingX[i] * (float)q;

	shade = 64.0f - (SSAFloat(light * 255 / 256) + 12.0f) * 32.0f / 128.0f;
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

	SSAInt blocks_skipped = skipped_by_thread(miny / q, thread);
	stack_y.store(miny + blocks_skipped * q);
	stack_dest.store(dest[blocks_skipped * q * pitch * pixelsize]);
	stack_subsectorGBuffer.store(subsectorGBuffer[blocks_skipped * q * pitch]);
	stack_posy_w.store(stack_posy_w.load() + gradWY * (q * blocks_skipped));
	for (int i = 0; i < TriVertex::NumVarying; i++)
		stack_posy_varying[i].store(stack_posy_varying[i].load() + gradVaryingY[i] * (blocks_skipped * q));

	SSAForBlock loop;
	y = stack_y.load();
	dest = stack_dest.load();
	subsectorGBuffer = stack_subsectorGBuffer.load();
	posy_w = stack_posy_w.load();
	for (int i = 0; i < TriVertex::NumVarying; i++)
		posy_varying[i] = stack_posy_varying[i].load();
	loop.loop_block(y < maxy, 0);
	{
		LoopBlockX();

		stack_posy_w.store(posy_w + gradWY * (q * thread.num_cores));
		for (int i = 0; i < TriVertex::NumVarying; i++)
			stack_posy_varying[i].store(posy_varying[i] + gradVaryingY[i] * (q * thread.num_cores));

		stack_dest.store(dest[q * pitch * pixelsize * thread.num_cores]);
		stack_subsectorGBuffer.store(subsectorGBuffer[q * pitch * thread.num_cores]);
		stack_y.store(y + thread.num_cores * q);
	}
	loop.end_block();
}

void DrawTriangleCodegen::LoopBlockX()
{
	stack_x.store(minx);
	stack_posx_w.store(posy_w);
	for (int i = 0; i < TriVertex::NumVarying; i++)
		stack_posx_varying[i].store(posy_varying[i]);

	SSAForBlock loop;
	x = stack_x.load();
	posx_w = stack_posx_w.load();
	for (int i = 0; i < TriVertex::NumVarying; i++)
		posx_varying[i] = stack_posx_varying[i].load();
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
		SSABool process_block = !(a == SSAInt(0) || b == SSAInt(0) || c == SSAInt(0));

		SetStencilBlock(x / 8 + y / 8 * stencilPitch);

		// Stencil test the whole block, if possible
		if (variant == TriDrawVariant::DrawSubsector || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector || variant == TriDrawVariant::StencilClose)
		{
			process_block = process_block && (!StencilIsSingleValue() || SSABool::compare_uge(StencilGetSingle(), stencilTestValue));
		}
		else
		{
			process_block = process_block && (!StencilIsSingleValue() || StencilGetSingle() == stencilTestValue);
		}

		SSAIfBlock branch;
		branch.if_block(process_block);

		// Check if block needs clipping
		SSABool clipneeded = (x + q) > clipright || (y + q) > clipbottom;

		SSAFloat globVis = SSAFloat(1706.0f);
		SSAFloat vis = globVis * posx_w;
		SSAInt lightscale = SSAInt(SSAFloat::clamp((shade - SSAFloat::MIN(SSAFloat(24.0f), vis)) / 32.0f, SSAFloat(0.0f), SSAFloat(31.0f / 32.0f)) * 256.0f, true);
		SSAInt diminishedlight = 256 - lightscale;

		if (!truecolor)
		{
			SSAInt diminishedindex = lightscale / 8;
			SSAInt lightindex = SSAInt::MIN((256 - light) * 32 / 256, SSAInt(31));
			SSAInt colormapindex = (!is_fixed_light).select(diminishedindex, lightindex);
			currentcolormap = Colormaps[colormapindex << 8];
		}
		else
		{
			currentlight = (!is_fixed_light).select(diminishedlight, light);
		}

		SSABool covered = a == SSAInt(0xF) && b == SSAInt(0xF) && c == SSAInt(0xF) && !clipneeded && StencilIsSingleValue();

		// Accept whole block when totally covered
		SSAIfBlock branch_covered;
		branch_covered.if_block(covered);
		{
			LoopFullBlock();
		}
		branch_covered.else_block();
		{
			SSAIfBlock branch_covered_stencil;
			branch_covered_stencil.if_block(StencilIsSingleValue());
			{
				SSABool stenciltestpass;
				if (variant == TriDrawVariant::DrawSubsector || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector || variant == TriDrawVariant::StencilClose)
				{
					stenciltestpass = SSABool::compare_uge(StencilGetSingle(), stencilTestValue);
				}
				else
				{
					stenciltestpass = StencilGetSingle() == stencilTestValue;
				}

				SSAIfBlock branch_stenciltestpass;
				branch_stenciltestpass.if_block(stenciltestpass);
				{
					LoopPartialBlock(true);
				}
				branch_stenciltestpass.end_block();
			}
			branch_covered_stencil.else_block();
			{
				LoopPartialBlock(false);
			}
			branch_covered_stencil.end_block();
		}
		branch_covered.end_block();

		branch.end_block();

		stack_posx_w.store(posx_w + gradWX);
		for (int i = 0; i < TriVertex::NumVarying; i++)
			stack_posx_varying[i].store(posx_varying[i] + gradVaryingX[i]);

		stack_x.store(x + q);
	}
	loop.end_block();
}

void DrawTriangleCodegen::SetupAffineBlock()
{
	SSAFloat rcpW0 = (float)0x01000000 / AffineW;
	SSAFloat rcpW1 = (float)0x01000000 / (AffineW + gradWX);

	for (int i = 0; i < TriVertex::NumVarying; i++)
	{
		AffineVaryingPosX[i] = SSAInt(AffineVaryingPosY[i] * rcpW0, false);
		AffineVaryingStepX[i] = (SSAInt((AffineVaryingPosY[i] + gradVaryingX[i]) * rcpW1, false) - AffineVaryingPosX[i]) / q;
	}
}

void DrawTriangleCodegen::LoopFullBlock()
{
	if (variant == TriDrawVariant::Stencil)
	{
		StencilClear(stencilWriteValue);
	}
	else if (variant == TriDrawVariant::StencilClose)
	{
		StencilClear(stencilWriteValue);
		for (int iy = 0; iy < q; iy++)
		{
			SSAIntPtr subsectorbuffer = subsectorGBuffer[x + iy * pitch];
			for (int ix = 0; ix < q; ix += 4)
			{
				subsectorbuffer[ix].store_unaligned_vec4i(SSAVec4i(subsectorDepth));
			}
		}
	}
	else
	{
		int pixelsize = truecolor ? 4 : 1;

		AffineW = posx_w;
		for (int i = 0; i < TriVertex::NumVarying; i++)
			AffineVaryingPosY[i] = posx_varying[i];

		for (int iy = 0; iy < q; iy++)
		{
			SSAUBytePtr buffer = dest[(x + iy * pitch) * pixelsize];
			SSAIntPtr subsectorbuffer = subsectorGBuffer[x + iy * pitch];

			SetupAffineBlock();

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
							pixels[sse] = subsectorTest.select(ProcessPixel32(pixels[sse], AffineVaryingPosX), pixels[sse]);
						}
						else
						{
							pixels[sse] = ProcessPixel32(pixels[sse], AffineVaryingPosX);
						}

						for (int i = 0; i < TriVertex::NumVarying; i++)
							AffineVaryingPosX[i] = AffineVaryingPosX[i] + AffineVaryingStepX[i];
					}

					buf.store_unaligned_vec16ub(SSAVec16ub(SSAVec8s(pixels[0], pixels[1]), SSAVec8s(pixels[2], pixels[3])));
				}
				else
				{
					SSAVec4i pixelsvec = buf.load_vec4ub(false);
					SSAInt pixels[4] =
					{
						pixelsvec[0],
						pixelsvec[1],
						pixelsvec[2],
						pixelsvec[3]
					};

					for (int sse = 0; sse < 4; sse++)
					{
						if (variant == TriDrawVariant::DrawSubsector || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
						{
							SSABool subsectorTest = subsectorbuffer[ix].load(true) >= subsectorDepth;
							pixels[sse] = subsectorTest.select(ProcessPixel8(pixels[sse], AffineVaryingPosX), pixels[sse]);
						}
						else
						{
							pixels[sse] = ProcessPixel8(pixels[sse], AffineVaryingPosX);
						}

						for (int i = 0; i < TriVertex::NumVarying; i++)
							AffineVaryingPosX[i] = AffineVaryingPosX[i] + AffineVaryingStepX[i];
					}

					buf.store_vec4ub(SSAVec4i(pixels[0], pixels[1], pixels[2], pixels[3]));
				}

				if (variant != TriDrawVariant::DrawSubsector && variant != TriDrawVariant::FillSubsector && variant != TriDrawVariant::FuzzSubsector)
					subsectorbuffer[ix].store_unaligned_vec4i(SSAVec4i(subsectorDepth));
			}

			AffineW = AffineW + gradWY;
			for (int i = 0; i < TriVertex::NumVarying; i++)
				AffineVaryingPosY[i] = AffineVaryingPosY[i] + gradVaryingY[i];
		}
	}
}

void DrawTriangleCodegen::LoopPartialBlock(bool isSingleStencilValue)
{
	int pixelsize = truecolor ? 4 : 1;

	if (variant == TriDrawVariant::Stencil || variant == TriDrawVariant::StencilClose)
	{
		if (isSingleStencilValue)
		{
			SSAInt stencilMask = StencilBlockMask.load(false);
			SSAUByte val0 = stencilMask.trunc_ubyte();
			for (int i = 0; i < 8 * 8; i++)
				StencilBlock[i].store(val0);
			StencilBlockMask.store(SSAInt(0));
		}

		SSAUByte lastStencilValue = StencilBlock[0].load(false);
		stack_stencilblock_restored.store(SSABool(true));
		stack_stencilblock_lastval.store(lastStencilValue);
	}

	stack_CY1.store(C1 + DX12 * y0 - DY12 * x0);
	stack_CY2.store(C2 + DX23 * y0 - DY23 * x0);
	stack_CY3.store(C3 + DX31 * y0 - DY31 * x0);
	stack_iy.store(SSAInt(0));
	stack_buffer.store(dest[x * pixelsize]);
	stack_subsectorbuffer.store(subsectorGBuffer[x]);
	stack_AffineW.store(posx_w);
	for (int i = 0; i < TriVertex::NumVarying; i++)
	{
		stack_AffineVaryingPosY[i].store(posx_varying[i]);
	}

	SSAForBlock loopy;
	SSAInt iy = stack_iy.load();
	SSAUBytePtr buffer = stack_buffer.load();
	SSAIntPtr subsectorbuffer = stack_subsectorbuffer.load();
	SSAInt CY1 = stack_CY1.load();
	SSAInt CY2 = stack_CY2.load();
	SSAInt CY3 = stack_CY3.load();
	AffineW = stack_AffineW.load();
	for (int i = 0; i < TriVertex::NumVarying; i++)
		AffineVaryingPosY[i] = stack_AffineVaryingPosY[i].load();
	loopy.loop_block(iy < SSAInt(q), q);
	{
		SetupAffineBlock();

		for (int i = 0; i < TriVertex::NumVarying; i++)
			stack_AffineVaryingPosX[i].store(AffineVaryingPosX[i]);

		stack_CX1.store(CY1);
		stack_CX2.store(CY2);
		stack_CX3.store(CY3);
		stack_ix.store(SSAInt(0));

		SSAForBlock loopx;
		SSABool stencilblock_restored;
		SSAUByte lastStencilValue;
		if (variant == TriDrawVariant::Stencil || variant == TriDrawVariant::StencilClose)
		{
			stencilblock_restored = stack_stencilblock_restored.load();
			lastStencilValue = stack_stencilblock_lastval.load();
		}
		SSAInt ix = stack_ix.load();
		SSAInt CX1 = stack_CX1.load();
		SSAInt CX2 = stack_CX2.load();
		SSAInt CX3 = stack_CX3.load();
		for (int i = 0; i < TriVertex::NumVarying; i++)
			AffineVaryingPosX[i] = stack_AffineVaryingPosX[i].load();
		loopx.loop_block(ix < SSAInt(q), q);
		{
			SSABool visible = (ix + x < clipright) && (iy + y < clipbottom);
			SSABool covered = CX1 > SSAInt(0) && CX2 > SSAInt(0) && CX3 > SSAInt(0) && visible;

			if (!isSingleStencilValue)
			{
				SSAUByte stencilValue = StencilBlock[ix + iy * 8].load(false);

				if (variant == TriDrawVariant::DrawSubsector || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
				{
					covered = covered && SSABool::compare_uge(stencilValue, stencilTestValue) && subsectorbuffer[ix].load(true) >= subsectorDepth;
				}
				else if (variant == TriDrawVariant::StencilClose)
				{
					covered = covered && SSABool::compare_uge(stencilValue, stencilTestValue);
				}
				else
				{
					covered = covered && stencilValue == stencilTestValue;
				}
			}
			else if (variant == TriDrawVariant::DrawSubsector || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
			{
				covered = covered && subsectorbuffer[ix].load(true) >= subsectorDepth;
			}

			SSAIfBlock branch;
			branch.if_block(covered);
			{
				if (variant == TriDrawVariant::Stencil)
				{
					StencilBlock[ix + iy * 8].store(stencilWriteValue);
				}
				else if (variant == TriDrawVariant::StencilClose)
				{
					StencilBlock[ix + iy * 8].store(stencilWriteValue);
					subsectorbuffer[ix].store(subsectorDepth);
				}
				else
				{
					SSAUBytePtr buf = buffer[ix * pixelsize];

					if (truecolor)
					{
						SSAVec4i bg = buf.load_vec4ub(false);
						buf.store_vec4ub(ProcessPixel32(bg, AffineVaryingPosX));
					}
					else
					{
						SSAUByte bg = buf.load(false);
						buf.store(ProcessPixel8(bg.zext_int(), AffineVaryingPosX).trunc_ubyte());
					}

					if (variant != TriDrawVariant::DrawSubsector && variant != TriDrawVariant::FillSubsector && variant != TriDrawVariant::FuzzSubsector)
						subsectorbuffer[ix].store(subsectorDepth);
				}
			}
			branch.end_block();

			if (variant == TriDrawVariant::Stencil || variant == TriDrawVariant::StencilClose)
			{
				SSAUByte newStencilValue = StencilBlock[ix + iy * 8].load(false);
				stack_stencilblock_restored.store(stencilblock_restored && newStencilValue == lastStencilValue);
				stack_stencilblock_lastval.store(newStencilValue);
			}

			for (int i = 0; i < TriVertex::NumVarying; i++)
				stack_AffineVaryingPosX[i].store(AffineVaryingPosX[i] + AffineVaryingStepX[i]);

			stack_CX1.store(CX1 - FDY12);
			stack_CX2.store(CX2 - FDY23);
			stack_CX3.store(CX3 - FDY31);
			stack_ix.store(ix + 1);
		}
		loopx.end_block();

		stack_AffineW.store(AffineW + gradWY);
		for (int i = 0; i < TriVertex::NumVarying; i++)
			stack_AffineVaryingPosY[i].store(AffineVaryingPosY[i] + gradVaryingY[i]);
		stack_CY1.store(CY1 + FDX12);
		stack_CY2.store(CY2 + FDX23);
		stack_CY3.store(CY3 + FDX31);
		stack_buffer.store(buffer[pitch * pixelsize]);
		stack_subsectorbuffer.store(subsectorbuffer[pitch]);
		stack_iy.store(iy + 1);
	}
	loopy.end_block();

	if (variant == TriDrawVariant::Stencil || variant == TriDrawVariant::StencilClose)
	{
		SSAIfBlock branch;
		SSABool restored = stack_stencilblock_restored.load();
		branch.if_block(restored);
		{
			SSAUByte lastStencilValue = stack_stencilblock_lastval.load();
			StencilClear(lastStencilValue);
		}
		branch.end_block();
	}
}

#if 0
void DrawTriangleCodegen::LoopMaskedStoreBlock()
{
	if (variant == TriDrawVariant::Stencil)
	{
	}
	else if (variant == TriDrawVariant::StencilClose)
	{
	}
	else
	{
		int pixelsize = truecolor ? 4 : 1;

		AffineW = posx_w;
		for (int i = 0; i < TriVertex::NumVarying; i++)
			AffineVaryingPosY[i] = posx_varying[i];

		SSAInt CY1 = C1 + DX12 * y0 - DY12 * x0;
		SSAInt CY2 = C2 + DX23 * y0 - DY23 * x0;
		SSAInt CY3 = C3 + DX31 * y0 - DY31 * x0;

		for (int iy = 0; iy < q; iy++)
		{
			SSAUBytePtr buffer = dest[(x + iy * pitch) * pixelsize];
			SSAIntPtr subsectorbuffer = subsectorGBuffer[x + iy * pitch];

			SetupAffineBlock();

			SSAInt CX1 = CY1;
			SSAInt CX2 = CY2;
			SSAInt CX3 = CY3;

			for (int ix = 0; ix < q; ix += 4)
			{
				SSABool covered[4];
				for (int maskindex = 0; maskindex < 4; maskindex++)
				{
					covered[maskindex] = CX1 > SSAInt(0) && CX2 > SSAInt(0) && CX3 > SSAInt(0);

					if (variant == TriDrawVariant::DrawSubsector || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
					{
						auto xx = SSAInt(ix + maskindex);
						auto yy = SSAInt(iy);
						covered[maskindex] = covered[maskindex] && SSABool::compare_uge(StencilGet(xx, yy), stencilTestValue) && subsectorbuffer[ix + maskindex].load(true) >= subsectorDepth;
					}
					else if (variant == TriDrawVariant::StencilClose)
					{
						auto xx = SSAInt(ix + maskindex);
						auto yy = SSAInt(iy);
						covered[maskindex] = covered[maskindex] && SSABool::compare_uge(StencilGet(xx, yy), stencilTestValue);
					}
					else
					{
						auto xx = SSAInt(ix + maskindex);
						auto yy = SSAInt(iy);
						covered[maskindex] = covered[maskindex] && StencilGet(xx, yy) == stencilTestValue;
					}

					CX1 = CX1 - FDY12;
					CX2 = CX2 - FDY23;
					CX3 = CX3 - FDY31;
				}

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
						pixels[sse] = ProcessPixel32(pixels[sse], AffineVaryingPosX);

						for (int i = 0; i < TriVertex::NumVarying; i++)
							AffineVaryingPosX[i] = AffineVaryingPosX[i] + AffineVaryingStepX[i];
					}

					buf.store_masked_vec16ub(SSAVec16ub(SSAVec8s(pixels[0], pixels[1]), SSAVec8s(pixels[2], pixels[3])), covered);
				}
				else
				{
					SSAVec4i pixelsvec = buf.load_vec4ub(false);
					SSAInt pixels[4] =
					{
						pixelsvec[0],
						pixelsvec[1],
						pixelsvec[2],
						pixelsvec[3]
					};

					for (int sse = 0; sse < 4; sse++)
					{
						pixels[sse] = ProcessPixel8(pixels[sse], AffineVaryingPosX);

						for (int i = 0; i < TriVertex::NumVarying; i++)
							AffineVaryingPosX[i] = AffineVaryingPosX[i] + AffineVaryingStepX[i];
					}

					buf.store_masked_vec4ub(SSAVec4i(pixels[0], pixels[1], pixels[2], pixels[3]), covered);
				}

				if (variant != TriDrawVariant::DrawSubsector && variant != TriDrawVariant::FillSubsector && variant != TriDrawVariant::FuzzSubsector)
					subsectorbuffer[ix].store_masked_vec4i(SSAVec4i(subsectorDepth), covered);
			}

			AffineW = AffineW + gradWY;
			for (int i = 0; i < TriVertex::NumVarying; i++)
				AffineVaryingPosY[i] = AffineVaryingPosY[i] + gradVaryingY[i];

			CY1 = CY1 + FDX12;
			CY2 = CY2 + FDX23;
			CY3 = CY3 + FDX31;
		}
	}
}
#endif

SSAVec4i DrawTriangleCodegen::TranslateSample32(SSAInt uvoffset)
{
	if (variant == TriDrawVariant::FillNormal || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
		return translation[color * 4].load_vec4ub(true);
	else
		return translation[texturePixels[uvoffset].load(true).zext_int() * 4].load_vec4ub(true);
}

SSAInt DrawTriangleCodegen::TranslateSample8(SSAInt uvoffset)
{
	if (variant == TriDrawVariant::FillNormal || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
		return translation[color].load(true).zext_int();
	else
		return translation[texturePixels[uvoffset].load(true).zext_int()].load(true).zext_int();
}

SSAVec4i DrawTriangleCodegen::Sample32(SSAInt uvoffset)
{
	if (variant == TriDrawVariant::FillNormal || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
		return SSAVec4i::unpack(color);
	else
		return texturePixels[uvoffset * 4].load_vec4ub(true);
}

SSAInt DrawTriangleCodegen::Sample8(SSAInt uvoffset)
{
	if (variant == TriDrawVariant::FillNormal || variant == TriDrawVariant::FillSubsector || variant == TriDrawVariant::FuzzSubsector)
		return color;
	else
		return texturePixels[uvoffset].load(true).zext_int();
}

SSAInt DrawTriangleCodegen::Shade8(SSAInt c)
{
	return currentcolormap[c].load(true).zext_int();
}

SSAVec4i DrawTriangleCodegen::ProcessPixel32(SSAVec4i bg, SSAInt *varying)
{
	SSAInt ufrac = varying[0] << 8;
	SSAInt vfrac = varying[1] << 8;

	SSAInt upos = ((ufrac >> 16) * textureWidth) >> 16;
	SSAInt vpos = ((vfrac >> 16) * textureHeight) >> 16;
	SSAInt uvoffset = upos * textureHeight + vpos;

	SSAVec4i fg;
	SSAVec4i output;

	switch (blendmode)
	{
	default:
	case TriBlendMode::Copy:
		fg = Sample32(uvoffset);
		output = blend_copy(shade_bgra_simple(fg, currentlight));
		break;
	case TriBlendMode::AlphaBlend:
		fg = Sample32(uvoffset);
		output = blend_alpha_blend(shade_bgra_simple(fg, currentlight), bg);
		break;
	case TriBlendMode::AddSolid:
		fg = Sample32(uvoffset);
		output = blend_add(shade_bgra_simple(fg, currentlight), bg, srcalpha, destalpha);
		break;
	case TriBlendMode::Add:
		fg = Sample32(uvoffset);
		output = blend_add(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::Sub:
		fg = Sample32(uvoffset);
		output = blend_sub(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::RevSub:
		fg = Sample32(uvoffset);
		output = blend_revsub(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::Stencil:
		fg = Sample32(uvoffset);
		output = blend_stencil(shade_bgra_simple(SSAVec4i::unpack(color), currentlight), fg[3], bg, srcalpha, destalpha);
		break;
	case TriBlendMode::Shaded:
		output = blend_stencil(shade_bgra_simple(SSAVec4i::unpack(color), currentlight), Sample8(uvoffset), bg, srcalpha, destalpha);
		break;
	case TriBlendMode::TranslateCopy:
		fg = TranslateSample32(uvoffset);
		output = blend_copy(shade_bgra_simple(fg, currentlight));
		break;
	case TriBlendMode::TranslateAlphaBlend:
		fg = TranslateSample32(uvoffset);
		output = blend_alpha_blend(shade_bgra_simple(fg, currentlight), bg);
		break;
	case TriBlendMode::TranslateAdd:
		fg = TranslateSample32(uvoffset);
		output = blend_add(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::TranslateSub:
		fg = TranslateSample32(uvoffset);
		output = blend_sub(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::TranslateRevSub:
		fg = TranslateSample32(uvoffset);
		output = blend_revsub(shade_bgra_simple(fg, currentlight), bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
		break;
	case TriBlendMode::AddSrcColorOneMinusSrcColor:
		fg = Sample32(uvoffset);
		output = blend_add_srccolor_oneminussrccolor(shade_bgra_simple(fg, currentlight), bg);
		break;
	case TriBlendMode::Skycap:
		fg = Sample32(uvoffset);
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
	return RGB32k[((c[2] >> 3) * 32 + (c[1] >> 3)) * 32 + (c[0] >> 3)].load(true).zext_int();
}

SSAInt DrawTriangleCodegen::ProcessPixel8(SSAInt bg, SSAInt *varying)
{
	SSAInt ufrac = varying[0] << 8;
	SSAInt vfrac = varying[1] << 8;

	SSAInt upos = ((ufrac >> 16) * textureWidth) >> 16;
	SSAInt vpos = ((vfrac >> 16) * textureHeight) >> 16;
	SSAInt uvoffset = upos * textureHeight + vpos;

	SSAVec4i fg;
	SSAInt alpha, inv_alpha;
	SSAInt output;
	SSAInt palindex;

	switch (blendmode)
	{
	default:
	case TriBlendMode::Copy:
		output = Shade8(Sample8(uvoffset));
		break;
	case TriBlendMode::AlphaBlend:
		palindex = Sample8(uvoffset);
		output = Shade8(palindex);
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::AddSolid:
		palindex = Sample8(uvoffset);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_add(fg, ToBgra(bg), srcalpha, destalpha));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::Add:
		palindex = Sample8(uvoffset);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_add(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::Sub:
		palindex = Sample8(uvoffset);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_sub(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::RevSub:
		palindex = Sample8(uvoffset);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_revsub(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::Stencil:
		output = ToPal8(blend_stencil(ToBgra(Shade8(color)), (Sample8(uvoffset) == SSAInt(0)).select(SSAInt(0), SSAInt(256)), ToBgra(bg), srcalpha, destalpha));
		break;
	case TriBlendMode::Shaded:
		palindex = Sample8(uvoffset);
		output = ToPal8(blend_stencil(ToBgra(Shade8(color)), palindex, ToBgra(bg), srcalpha, destalpha));
		break;
	case TriBlendMode::TranslateCopy:
		palindex = TranslateSample8(uvoffset);
		output = Shade8(palindex);
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::TranslateAlphaBlend:
		palindex = TranslateSample8(uvoffset);
		output = Shade8(palindex);
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::TranslateAdd:
		palindex = TranslateSample8(uvoffset);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_add(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::TranslateSub:
		palindex = TranslateSample8(uvoffset);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_sub(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::TranslateRevSub:
		palindex = TranslateSample8(uvoffset);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_revsub(fg, ToBgra(bg), srcalpha, calc_blend_bgalpha(fg, destalpha)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::AddSrcColorOneMinusSrcColor:
		palindex = Sample8(uvoffset);
		fg = ToBgra(Shade8(palindex));
		output = ToPal8(blend_add_srccolor_oneminussrccolor(fg, ToBgra(bg)));
		output = (palindex == SSAInt(0)).select(bg, output);
		break;
	case TriBlendMode::Skycap:
		fg = ToBgra(Sample8(uvoffset));
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

void DrawTriangleCodegen::SetStencilBlock(SSAInt block)
{
	StencilBlock = stencilValues[block * 64];
	StencilBlockMask = stencilMasks[block];
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
	clipright = args[0][6].load(true);
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
	if (!truecolor)
	{
		Colormaps = args[0][20].load(true);
		RGB32k = args[0][21].load(true);
		BaseColors = args[0][22].load(true);
	}

	thread.core = thread_data[0][0].load(true);
	thread.num_cores = thread_data[0][1].load(true);
	thread.pass_start_y = SSAInt(0);
	thread.pass_end_y = SSAInt(32000);
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
