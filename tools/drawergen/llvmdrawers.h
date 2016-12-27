/*
**  LLVM code generated drawers
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

#include "fixedfunction/drawspancodegen.h"
#include "fixedfunction/drawwallcodegen.h"
#include "fixedfunction/drawcolumncodegen.h"
#include "fixedfunction/drawskycodegen.h"
#include "fixedfunction/drawtrianglecodegen.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_scope.h"
#include "ssa/ssa_for_block.h"
#include "ssa/ssa_if_block.h"
#include "ssa/ssa_stack.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_struct_type.h"
#include "ssa/ssa_value.h"
#include "ssa/ssa_barycentric_weight.h"
#include "llvmprogram.h"
#include <iostream>

class LLVMDrawers
{
public:
	LLVMDrawers(const std::string &triple, const std::string &cpuName, const std::string &features, const std::string namePostfix);

	std::vector<uint8_t> ObjectFile;

private:
	void CodegenDrawColumn(const char *name, DrawColumnVariant variant);
	void CodegenDrawSpan(const char *name, DrawSpanVariant variant);
	void CodegenDrawWall(const char *name, DrawWallVariant variant);
	void CodegenDrawSky(const char *name, DrawSkyVariant variant);
	void CodegenDrawTriangle(const std::string &name, TriBlendMode blendmode, bool truecolor, bool colorfill);

	llvm::Type *GetDrawColumnArgsStruct(llvm::LLVMContext &context);
	llvm::Type *GetDrawSpanArgsStruct(llvm::LLVMContext &context);
	llvm::Type *GetDrawWallArgsStruct(llvm::LLVMContext &context);
	llvm::Type *GetDrawSkyArgsStruct(llvm::LLVMContext &context);
	llvm::Type *GetWorkerThreadDataStruct(llvm::LLVMContext &context);
	llvm::Type *GetTriLightStruct(llvm::LLVMContext &context);
	llvm::Type *GetTriVertexStruct(llvm::LLVMContext &context);
	llvm::Type *GetTriMatrixStruct(llvm::LLVMContext &context);
	llvm::Type *GetTriUniformsStruct(llvm::LLVMContext &context);
	llvm::Type *GetTriFullSpanStruct(llvm::LLVMContext &context);
	llvm::Type *GetTriPartialBlockStruct(llvm::LLVMContext &context);
	llvm::Type *GetTriDrawTriangleArgs(llvm::LLVMContext &context);

	llvm::Type *DrawColumnArgsStruct = nullptr;
	llvm::Type *DrawSpanArgsStruct = nullptr;
	llvm::Type *DrawWallArgsStruct = nullptr;
	llvm::Type *DrawSkyArgsStruct = nullptr;
	llvm::Type *WorkerThreadDataStruct = nullptr;
	llvm::Type *TriLightStruct = nullptr;
	llvm::Type *TriVertexStruct = nullptr;
	llvm::Type *TriMatrixStruct = nullptr;
	llvm::Type *TriUniformsStruct = nullptr;
	llvm::Type *TriFullSpanStruct = nullptr;
	llvm::Type *TriPartialBlockStruct = nullptr;
	llvm::Type *TriDrawTriangleArgs = nullptr;

	LLVMProgram mProgram;
	std::string mNamePostfix;
};
