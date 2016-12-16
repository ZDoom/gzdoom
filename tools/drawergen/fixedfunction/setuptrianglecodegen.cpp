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
#include "fixedfunction/setuptrianglecodegen.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_scope.h"
#include "ssa/ssa_for_block.h"
#include "ssa/ssa_if_block.h"
#include "ssa/ssa_stack.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_struct_type.h"
#include "ssa/ssa_value.h"

void SetupTriangleCodegen::Generate(bool subsectorTest, SSAValue args, SSAValue thread_data)
{
	this->subsectorTest = subsectorTest;
	LoadArgs(args, thread_data);
	Setup();
	LoopBlockY();
}

SSAInt SetupTriangleCodegen::FloatTo28_4(SSAFloat v)
{
	// SSAInt(SSAFloat::round(16.0f * v), false);
	SSAInt a = SSAInt(v * 32.0f, false);
	return (a + (a.ashr(31) | SSAInt(1))).ashr(1);
}

void SetupTriangleCodegen::Setup()
{
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
}

void SetupTriangleCodegen::LoopBlockY()
{
	SSAInt blocks_skipped = skipped_by_thread(miny / q, thread);
	stack_y.store(miny + blocks_skipped * q);
	stack_subsectorGBuffer.store(subsectorGBuffer[blocks_skipped * q * pitch]);

	SSAForBlock loop;
	y = stack_y.load();
	subsectorGBuffer = stack_subsectorGBuffer.load();
	loop.loop_block(y < maxy, 0);
	{
		LoopBlockX();

		stack_subsectorGBuffer.store(subsectorGBuffer[q * pitch * thread.num_cores]);
		stack_y.store(y + thread.num_cores * q);
	}
	loop.end_block();
}

void SetupTriangleCodegen::LoopBlockX()
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
		SSABool process_block = !(a == SSAInt(0) || b == SSAInt(0) || c == SSAInt(0));

		SetStencilBlock(x / 8 + y / 8 * stencilPitch);

		// Stencil test the whole block, if possible
		if (subsectorTest)
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
				if (subsectorTest)
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

		stack_x.store(x + q);
	}
	loop.end_block();
}

void SetupTriangleCodegen::LoopFullBlock()
{
	/*
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
	*/
}

void SetupTriangleCodegen::LoopPartialBlock(bool isSingleStencilValue)
{
	/*
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
	*/
}

void SetupTriangleCodegen::SetStencilBlock(SSAInt block)
{
	StencilBlock = stencilValues[block * 64];
	StencilBlockMask = stencilMasks[block];
}

SSAUByte SetupTriangleCodegen::StencilGetSingle()
{
	return StencilBlockMask.load(false).trunc_ubyte();
}

void SetupTriangleCodegen::StencilClear(SSAUByte value)
{
	StencilBlockMask.store(SSAInt(0xffffff00) | value.zext_int());
}

SSABool SetupTriangleCodegen::StencilIsSingleValue()
{
	return (StencilBlockMask.load(false) & SSAInt(0xffffff00)) == SSAInt(0xffffff00);
}

void SetupTriangleCodegen::LoadArgs(SSAValue args, SSAValue thread_data)
{
	pitch = args[0][1].load(true);
	v1 = LoadTriVertex(args[0][2].load(true));
	v2 = LoadTriVertex(args[0][3].load(true));
	v3 = LoadTriVertex(args[0][4].load(true));
	clipright = args[0][6].load(true);
	clipbottom = args[0][8].load(true);
	stencilValues = args[0][14].load(true);
	stencilMasks = args[0][15].load(true);
	stencilPitch = args[0][16].load(true);
	stencilTestValue = args[0][17].load(true);
	stencilWriteValue = args[0][18].load(true);
	subsectorGBuffer = args[0][19].load(true);

	thread.core = thread_data[0][0].load(true);
	thread.num_cores = thread_data[0][1].load(true);
	thread.pass_start_y = SSAInt(0);
	thread.pass_end_y = SSAInt(32000);
}

SSASetupVertex SetupTriangleCodegen::LoadTriVertex(SSAValue ptr)
{
	SSASetupVertex v;
	v.x = ptr[0][0].load(true);
	v.y = ptr[0][1].load(true);
	v.z = ptr[0][2].load(true);
	v.w = ptr[0][3].load(true);
	return v;
}
