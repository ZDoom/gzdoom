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

#include "precomp.h"
#include "timestamp.h"
#include "llvmdrawers.h"
#include "exception.h"

LLVMDrawers::LLVMDrawers(const std::string &triple, const std::string &cpuName, const std::string &features, const std::string namePostfix) : mNamePostfix(namePostfix)
{
	mProgram.CreateModule();

	CodegenDrawColumn("FillColumn", DrawColumnVariant::Fill);
	CodegenDrawColumn("FillColumnAdd", DrawColumnVariant::FillAdd);
	CodegenDrawColumn("FillColumnAddClamp", DrawColumnVariant::FillAddClamp);
	CodegenDrawColumn("FillColumnSubClamp", DrawColumnVariant::FillSubClamp);
	CodegenDrawColumn("FillColumnRevSubClamp", DrawColumnVariant::FillRevSubClamp);
	CodegenDrawColumn("DrawColumn", DrawColumnVariant::Draw);
	CodegenDrawColumn("DrawColumnAdd", DrawColumnVariant::DrawAdd);
	CodegenDrawColumn("DrawColumnShaded", DrawColumnVariant::DrawShaded);
	CodegenDrawColumn("DrawColumnAddClamp", DrawColumnVariant::DrawAddClamp);
	CodegenDrawColumn("DrawColumnSubClamp", DrawColumnVariant::DrawSubClamp);
	CodegenDrawColumn("DrawColumnRevSubClamp", DrawColumnVariant::DrawRevSubClamp);
	CodegenDrawColumn("DrawColumnTranslated", DrawColumnVariant::DrawTranslated);
	CodegenDrawColumn("DrawColumnTlatedAdd", DrawColumnVariant::DrawTlatedAdd);
	CodegenDrawColumn("DrawColumnAddClampTranslated", DrawColumnVariant::DrawAddClampTranslated);
	CodegenDrawColumn("DrawColumnSubClampTranslated", DrawColumnVariant::DrawSubClampTranslated);
	CodegenDrawColumn("DrawColumnRevSubClampTranslated", DrawColumnVariant::DrawRevSubClampTranslated);
	CodegenDrawSpan("DrawSpan", DrawSpanVariant::Opaque);
	CodegenDrawSpan("DrawSpanMasked", DrawSpanVariant::Masked);
	CodegenDrawSpan("DrawSpanTranslucent", DrawSpanVariant::Translucent);
	CodegenDrawSpan("DrawSpanMaskedTranslucent", DrawSpanVariant::MaskedTranslucent);
	CodegenDrawSpan("DrawSpanAddClamp", DrawSpanVariant::AddClamp);
	CodegenDrawSpan("DrawSpanMaskedAddClamp", DrawSpanVariant::MaskedAddClamp);
	CodegenDrawWall("vlinec1", DrawWallVariant::Opaque);
	CodegenDrawWall("mvlinec1", DrawWallVariant::Masked);
	CodegenDrawWall("tmvline1_add", DrawWallVariant::Add);
	CodegenDrawWall("tmvline1_addclamp", DrawWallVariant::AddClamp);
	CodegenDrawWall("tmvline1_subclamp", DrawWallVariant::SubClamp);
	CodegenDrawWall("tmvline1_revsubclamp", DrawWallVariant::RevSubClamp);
	CodegenDrawSky("DrawSky1", DrawSkyVariant::Single);
	CodegenDrawSky("DrawDoubleSky1", DrawSkyVariant::Double);
	for (int i = 0; i < NumTriBlendModes(); i++)
	{
		CodegenDrawTriangle("TriDraw8_" + std::to_string(i), (TriBlendMode)i, false, false);
		CodegenDrawTriangle("TriDraw32_" + std::to_string(i), (TriBlendMode)i, true, false);
		CodegenDrawTriangle("TriFill8_" + std::to_string(i), (TriBlendMode)i, false, true);
		CodegenDrawTriangle("TriFill32_" + std::to_string(i), (TriBlendMode)i, true, true);
	}

	ObjectFile = mProgram.GenerateObjectFile(triple, cpuName, features);
}

void LLVMDrawers::CodegenDrawColumn(const char *name, DrawColumnVariant variant)
{
	llvm::IRBuilder<> builder(mProgram.context());
	SSAScope ssa_scope(&mProgram.context(), mProgram.module(), &builder);

	SSAFunction function(name + mNamePostfix);
	function.add_parameter(GetDrawColumnArgsStruct(mProgram.context()));
	function.add_parameter(GetWorkerThreadDataStruct(mProgram.context()));
	function.create_public();

	DrawColumnCodegen codegen;
	codegen.Generate(variant, function.parameter(0), function.parameter(1));

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		throw Exception("verifyFunction failed for CodegenDrawColumn()");
}

void LLVMDrawers::CodegenDrawSpan(const char *name, DrawSpanVariant variant)
{
	llvm::IRBuilder<> builder(mProgram.context());
	SSAScope ssa_scope(&mProgram.context(), mProgram.module(), &builder);

	SSAFunction function(name + mNamePostfix);
	function.add_parameter(GetDrawSpanArgsStruct(mProgram.context()));
	function.create_public();

	DrawSpanCodegen codegen;
	codegen.Generate(variant, function.parameter(0));

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		throw Exception("verifyFunction failed for CodegenDrawSpan()");
}

void LLVMDrawers::CodegenDrawWall(const char *name, DrawWallVariant variant)
{
	llvm::IRBuilder<> builder(mProgram.context());
	SSAScope ssa_scope(&mProgram.context(), mProgram.module(), &builder);

	SSAFunction function(name + mNamePostfix);
	function.add_parameter(GetDrawWallArgsStruct(mProgram.context()));
	function.add_parameter(GetWorkerThreadDataStruct(mProgram.context()));
	function.create_public();

	DrawWallCodegen codegen;
	codegen.Generate(variant, function.parameter(0), function.parameter(1));

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		throw Exception("verifyFunction failed for CodegenDrawWall()");
}

void LLVMDrawers::CodegenDrawSky(const char *name, DrawSkyVariant variant)
{
	llvm::IRBuilder<> builder(mProgram.context());
	SSAScope ssa_scope(&mProgram.context(), mProgram.module(), &builder);

	SSAFunction function(name + mNamePostfix);
	function.add_parameter(GetDrawSkyArgsStruct(mProgram.context()));
	function.add_parameter(GetWorkerThreadDataStruct(mProgram.context()));
	function.create_public();

	DrawSkyCodegen codegen;
	codegen.Generate(variant, function.parameter(0), function.parameter(1));

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		throw Exception("verifyFunction failed for CodegenDrawSky()");
}

void LLVMDrawers::CodegenDrawTriangle(const std::string &name, TriBlendMode blendmode, bool truecolor, bool colorfill)
{
	llvm::IRBuilder<> builder(mProgram.context());
	SSAScope ssa_scope(&mProgram.context(), mProgram.module(), &builder);

	SSAFunction function(name + mNamePostfix);
	function.add_parameter(GetTriDrawTriangleArgs(mProgram.context()));
	function.add_parameter(GetWorkerThreadDataStruct(mProgram.context()));
	function.create_public();

	DrawTriangleCodegen codegen;
	codegen.Generate(blendmode, truecolor, colorfill, function.parameter(0), function.parameter(1));

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		throw Exception("verifyFunction failed for CodegenDrawTriangle()");
}

llvm::Type *LLVMDrawers::GetDrawColumnArgsStruct(llvm::LLVMContext &context)
{
	if (DrawColumnArgsStruct)
		return DrawColumnArgsStruct;

	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // uint32_t *dest;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // const uint32_t *source;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // const uint32_t *source2;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // uint8_t *colormap;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // uint8_t *translation;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // const uint32_t *basecolors;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t pitch;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t count;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t dest_y;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t iscale;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t texturefracx;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t textureheight;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t texturefrac;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t light;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t color;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t srccolor;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t srcalpha;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t destalpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_alpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_red;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_green;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_blue;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_alpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_red;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_green;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_blue;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t desaturate;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t flags;
	DrawColumnArgsStruct = llvm::StructType::create(context, elements, "DrawColumnArgs", false)->getPointerTo();
	return DrawColumnArgsStruct;
}

llvm::Type *LLVMDrawers::GetDrawSpanArgsStruct(llvm::LLVMContext &context)
{
	if (DrawSpanArgsStruct)
		return DrawSpanArgsStruct;

	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // uint8_t *destorg;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // const uint32_t *source;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t destpitch;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t xfrac;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t yfrac;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t xstep;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t ystep;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t x1;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t x2;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t y;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t xbits;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t ybits;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t light;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t srcalpha;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t destalpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_alpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_red;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_green;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_blue;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_alpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_red;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_green;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_blue;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t desaturate;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t flags;
	elements.push_back(llvm::Type::getFloatTy(context)); // float viewpos_x;
	elements.push_back(llvm::Type::getFloatTy(context)); // float step_viewpos_x;
	elements.push_back(GetTriLightStruct(context));      // TriLight *dynlights;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t num_dynlights;
	DrawSpanArgsStruct = llvm::StructType::create(context, elements, "DrawSpanArgs", false)->getPointerTo();
	return DrawSpanArgsStruct;
}

llvm::Type *LLVMDrawers::GetDrawWallArgsStruct(llvm::LLVMContext &context)
{
	if (DrawWallArgsStruct)
		return DrawWallArgsStruct;

	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt8PtrTy(context));
	for (int i = 0; i < 8; i++)
		elements.push_back(llvm::Type::getInt8PtrTy(context));
	for (int i = 0; i < 25; i++)
		elements.push_back(llvm::Type::getInt32Ty(context));
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_alpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_red;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_green;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_blue;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_alpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_red;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_green;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_blue;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t desaturate;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t flags;
	elements.push_back(llvm::Type::getFloatTy(context)); // float z;
	elements.push_back(llvm::Type::getFloatTy(context)); // float step_z;
	elements.push_back(GetTriLightStruct(context));      // TriLight *dynlights;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t num_dynlights;

	DrawWallArgsStruct = llvm::StructType::create(context, elements, "DrawWallArgs", false)->getPointerTo();
	return DrawWallArgsStruct;
}

llvm::Type *LLVMDrawers::GetDrawSkyArgsStruct(llvm::LLVMContext &context)
{
	if (DrawSkyArgsStruct)
		return DrawSkyArgsStruct;

	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt8PtrTy(context));
	for (int i = 0; i < 8; i++)
		elements.push_back(llvm::Type::getInt8PtrTy(context));
	for (int i = 0; i < 16; i++)
		elements.push_back(llvm::Type::getInt32Ty(context));
	DrawSkyArgsStruct = llvm::StructType::create(context, elements, "DrawSkyArgs", false)->getPointerTo();
	return DrawSkyArgsStruct;
}

llvm::Type *LLVMDrawers::GetWorkerThreadDataStruct(llvm::LLVMContext &context)
{
	if (WorkerThreadDataStruct)
		return WorkerThreadDataStruct;

	std::vector<llvm::Type *> elements;
	for (int i = 0; i < 4; i++)
		elements.push_back(llvm::Type::getInt32Ty(context));
	elements.push_back(llvm::Type::getInt8PtrTy(context));
	elements.push_back(GetTriFullSpanStruct(context));
	elements.push_back(GetTriPartialBlockStruct(context));
	for (int i = 0; i < 4; i++)
		elements.push_back(llvm::Type::getInt32Ty(context));
	WorkerThreadDataStruct = llvm::StructType::create(context, elements, "ThreadData", false)->getPointerTo();
	return WorkerThreadDataStruct;
}

llvm::Type *LLVMDrawers::GetTriLightStruct(llvm::LLVMContext &context)
{
	if (TriLightStruct)
		return TriLightStruct;

	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt32Ty(context));
	for (int i = 0; i < 4; i++)
		elements.push_back(llvm::Type::getFloatTy(context));
	TriLightStruct = llvm::StructType::create(context, elements, "TriLight", false)->getPointerTo();
	return TriLightStruct;
}

llvm::Type *LLVMDrawers::GetTriVertexStruct(llvm::LLVMContext &context)
{
	if (TriVertexStruct)
		return TriVertexStruct;

	std::vector<llvm::Type *> elements;
	for (int i = 0; i < 4 + TriVertex::NumVarying; i++)
		elements.push_back(llvm::Type::getFloatTy(context));
	TriVertexStruct = llvm::StructType::create(context, elements, "TriVertex", false)->getPointerTo();
	return TriVertexStruct;
}

llvm::Type *LLVMDrawers::GetTriMatrixStruct(llvm::LLVMContext &context)
{
	if (TriMatrixStruct)
		return TriMatrixStruct;

	std::vector<llvm::Type *> elements;
	for (int i = 0; i < 4 * 4; i++)
		elements.push_back(llvm::Type::getFloatTy(context));
	TriMatrixStruct = llvm::StructType::create(context, elements, "TriMatrix", false)->getPointerTo();
	return TriMatrixStruct;
}

llvm::Type *LLVMDrawers::GetTriUniformsStruct(llvm::LLVMContext &context)
{
	if (TriUniformsStruct)
		return TriUniformsStruct;

	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t light;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t subsectorDepth;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t color;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t srcalpha;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t destalpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_alpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_red;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_green;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t light_blue;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_alpha;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_red;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_green;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t fade_blue;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint16_t desaturate;
	elements.push_back(llvm::Type::getFloatTy(context)); // float globvis;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t flags;
	elements.push_back(GetTriMatrixStruct(context));     // TriMatrix objectToClip
	TriUniformsStruct = llvm::StructType::create(context, elements, "TriUniforms", false)->getPointerTo();
	return TriUniformsStruct;
}

llvm::Type *LLVMDrawers::GetTriFullSpanStruct(llvm::LLVMContext &context)
{
	if (TriFullSpanStruct)
		return TriFullSpanStruct;

	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint32_t X;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint32_t Y;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t Length;
	TriFullSpanStruct = llvm::StructType::create(context, elements, "TriFullSpan", false)->getPointerTo();
	return TriFullSpanStruct;
}

llvm::Type *LLVMDrawers::GetTriPartialBlockStruct(llvm::LLVMContext &context)
{
	if (TriPartialBlockStruct)
		return TriPartialBlockStruct;

	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint32_t X;
	elements.push_back(llvm::Type::getInt16Ty(context)); // uint32_t Y;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t Mask0;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t Mask1;
	TriPartialBlockStruct = llvm::StructType::create(context, elements, "TriPartialBlock", false)->getPointerTo();
	return TriPartialBlockStruct;
}

llvm::Type *LLVMDrawers::GetTriDrawTriangleArgs(llvm::LLVMContext &context)
{
	if (TriDrawTriangleArgs)
		return TriDrawTriangleArgs;

	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt8PtrTy(context));  // uint8_t *dest;
	elements.push_back(llvm::Type::getInt32Ty(context));    // int32_t pitch;
	elements.push_back(GetTriVertexStruct(context)); // TriVertex *v1;
	elements.push_back(GetTriVertexStruct(context)); // TriVertex *v2;
	elements.push_back(GetTriVertexStruct(context)); // TriVertex *v3;
	elements.push_back(llvm::Type::getInt32Ty(context));    // int32_t clipleft;
	elements.push_back(llvm::Type::getInt32Ty(context));    // int32_t clipright;
	elements.push_back(llvm::Type::getInt32Ty(context));    // int32_t cliptop;
	elements.push_back(llvm::Type::getInt32Ty(context));    // int32_t clipbottom;
	elements.push_back(llvm::Type::getInt8PtrTy(context));  // const uint8_t *texturePixels;
	elements.push_back(llvm::Type::getInt32Ty(context));    // uint32_t textureWidth;
	elements.push_back(llvm::Type::getInt32Ty(context));    // uint32_t textureHeight;
	elements.push_back(llvm::Type::getInt8PtrTy(context));  // const uint8_t *translation;
	elements.push_back(GetTriUniformsStruct(context)); // const TriUniforms *uniforms;
	elements.push_back(llvm::Type::getInt8PtrTy(context));  // uint8_t *stencilValues;
	elements.push_back(llvm::Type::getInt32PtrTy(context)); // uint32_t *stencilMasks;
	elements.push_back(llvm::Type::getInt32Ty(context));    // int32_t stencilPitch;
	elements.push_back(llvm::Type::getInt8Ty(context));     // uint8_t stencilTestValue;
	elements.push_back(llvm::Type::getInt8Ty(context));     // uint8_t stencilWriteValue;
	elements.push_back(llvm::Type::getInt32PtrTy(context)); // uint32_t *subsectorGBuffer;
	elements.push_back(llvm::Type::getInt8PtrTy(context));  // const uint8_t *colormaps;
	elements.push_back(llvm::Type::getInt8PtrTy(context));  // const uint8_t *RGB256k;
	elements.push_back(llvm::Type::getInt8PtrTy(context));  // const uint8_t *BaseColors;
	TriDrawTriangleArgs = llvm::StructType::create(context, elements, "TriDrawTriangle", false)->getPointerTo();
	return TriDrawTriangleArgs;
}
