
#include "i_system.h"
#include "r_compiler/llvm_include.h"
#include "r_compiler/fixedfunction/drawspancodegen.h"
#include "r_compiler/fixedfunction/drawwallcodegen.h"
#include "r_compiler/fixedfunction/drawcolumncodegen.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"
#include "r_compiler/ssa/ssa_barycentric_weight.h"

class LLVMProgram
{
public:
	LLVMProgram();
	~LLVMProgram();

	void StopLogFatalErrors();

	template<typename Func>
	Func *GetProcAddress(const char *name) { return reinterpret_cast<Func*>(PointerToFunction(name)); }

	llvm::LLVMContext &context() { return *mContext; }
	llvm::Module *module() { return mModule; }
	llvm::ExecutionEngine *engine() { return mEngine.get(); }
	llvm::legacy::PassManager *modulePassManager() { return mModulePassManager.get(); }
	llvm::legacy::FunctionPassManager *functionPassManager() { return mFunctionPassManager.get(); }

private:
	void *PointerToFunction(const char *name);

	std::unique_ptr<llvm::LLVMContext> mContext;
	llvm::Module *mModule;
	std::unique_ptr<llvm::ExecutionEngine> mEngine;
	std::unique_ptr<llvm::legacy::PassManager> mModulePassManager;
	std::unique_ptr<llvm::legacy::FunctionPassManager> mFunctionPassManager;
};

class LLVMDrawersImpl : public LLVMDrawers
{
public:
	LLVMDrawersImpl();

private:
	void CodegenDrawColumn(const char *name, DrawColumnVariant variant, DrawColumnMethod method);
	void CodegenDrawSpan(const char *name, DrawSpanVariant variant);
	void CodegenDrawWall(const char *name, DrawWallVariant variant, int columns);

	static llvm::Type *GetDrawColumnArgsStruct(llvm::LLVMContext &context);
	static llvm::Type *GetDrawSpanArgsStruct(llvm::LLVMContext &context);
	static llvm::Type *GetDrawWallArgsStruct(llvm::LLVMContext &context);
	static llvm::Type *GetWorkerThreadDataStruct(llvm::LLVMContext &context);

	LLVMProgram mProgram;
};

/////////////////////////////////////////////////////////////////////////////

LLVMDrawers *LLVMDrawers::Singleton = nullptr;

void LLVMDrawers::Create()
{
	if (!Singleton)
		Singleton = new LLVMDrawersImpl();
}

void LLVMDrawers::Destroy()
{
	delete Singleton;
	Singleton = nullptr;
}

LLVMDrawers *LLVMDrawers::Instance()
{
	return Singleton;
}

/////////////////////////////////////////////////////////////////////////////

LLVMDrawersImpl::LLVMDrawersImpl()
{
	CodegenDrawColumn("FillColumn", DrawColumnVariant::Fill, DrawColumnMethod::Normal);
	CodegenDrawColumn("FillColumnAdd", DrawColumnVariant::FillAdd, DrawColumnMethod::Normal);
	CodegenDrawColumn("FillColumnAddClamp", DrawColumnVariant::FillAddClamp, DrawColumnMethod::Normal);
	CodegenDrawColumn("FillColumnSubClamp", DrawColumnVariant::FillSubClamp, DrawColumnMethod::Normal);
	CodegenDrawColumn("FillColumnRevSubClamp", DrawColumnVariant::FillRevSubClamp, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumn", DrawColumnVariant::Draw, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnAdd", DrawColumnVariant::DrawAdd, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnShaded", DrawColumnVariant::DrawShaded, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnAddClamp", DrawColumnVariant::DrawAddClamp, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnSubClamp", DrawColumnVariant::DrawSubClamp, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnRevSubClamp", DrawColumnVariant::DrawRevSubClamp, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnTranslated", DrawColumnVariant::DrawTranslated, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnTlatedAdd", DrawColumnVariant::DrawTlatedAdd, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnAddClampTranslated", DrawColumnVariant::DrawAddClampTranslated, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnSubClampTranslated", DrawColumnVariant::DrawSubClampTranslated, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnRevSubClampTranslated", DrawColumnVariant::DrawRevSubClampTranslated, DrawColumnMethod::Normal);
	CodegenDrawColumn("DrawColumnRt1", DrawColumnVariant::Draw, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1Copy", DrawColumnVariant::DrawCopy, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1Add", DrawColumnVariant::DrawAdd, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1Shaded", DrawColumnVariant::DrawShaded, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1AddClamp", DrawColumnVariant::DrawAddClamp, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1SubClamp", DrawColumnVariant::DrawSubClamp, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1RevSubClamp", DrawColumnVariant::DrawRevSubClamp, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1Translated", DrawColumnVariant::DrawTranslated, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1TlatedAdd", DrawColumnVariant::DrawTlatedAdd, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1AddClampTranslated", DrawColumnVariant::DrawAddClampTranslated, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1SubClampTranslated", DrawColumnVariant::DrawSubClampTranslated, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt1RevSubClampTranslated", DrawColumnVariant::DrawRevSubClampTranslated, DrawColumnMethod::Rt1);
	CodegenDrawColumn("DrawColumnRt4", DrawColumnVariant::Draw, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4Copy", DrawColumnVariant::DrawCopy, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4Add", DrawColumnVariant::DrawAdd, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4Shaded", DrawColumnVariant::DrawShaded, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4AddClamp", DrawColumnVariant::DrawAddClamp, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4SubClamp", DrawColumnVariant::DrawSubClamp, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4RevSubClamp", DrawColumnVariant::DrawRevSubClamp, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4Translated", DrawColumnVariant::DrawTranslated, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4TlatedAdd", DrawColumnVariant::DrawTlatedAdd, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4AddClampTranslated", DrawColumnVariant::DrawAddClampTranslated, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4SubClampTranslated", DrawColumnVariant::DrawSubClampTranslated, DrawColumnMethod::Rt4);
	CodegenDrawColumn("DrawColumnRt4RevSubClampTranslated", DrawColumnVariant::DrawRevSubClampTranslated, DrawColumnMethod::Rt4);
	CodegenDrawSpan("DrawSpan", DrawSpanVariant::Opaque);
	CodegenDrawSpan("DrawSpanMasked", DrawSpanVariant::Masked);
	CodegenDrawSpan("DrawSpanTranslucent", DrawSpanVariant::Translucent);
	CodegenDrawSpan("DrawSpanMaskedTranslucent", DrawSpanVariant::MaskedTranslucent);
	CodegenDrawSpan("DrawSpanAddClamp", DrawSpanVariant::AddClamp);
	CodegenDrawSpan("DrawSpanMaskedAddClamp", DrawSpanVariant::MaskedAddClamp);
	CodegenDrawWall("vlinec1", DrawWallVariant::Opaque, 1);
	CodegenDrawWall("vlinec4", DrawWallVariant::Opaque, 4);
	CodegenDrawWall("mvlinec1", DrawWallVariant::Masked, 1);
	CodegenDrawWall("mvlinec4", DrawWallVariant::Masked, 4);
	CodegenDrawWall("tmvline1_add", DrawWallVariant::Add, 1);
	CodegenDrawWall("tmvline4_add", DrawWallVariant::Add, 4);
	CodegenDrawWall("tmvline1_addclamp", DrawWallVariant::AddClamp, 1);
	CodegenDrawWall("tmvline4_addclamp", DrawWallVariant::AddClamp, 4);
	CodegenDrawWall("tmvline1_subclamp", DrawWallVariant::SubClamp, 1);
	CodegenDrawWall("tmvline4_subclamp", DrawWallVariant::SubClamp, 4);
	CodegenDrawWall("tmvline1_revsubclamp", DrawWallVariant::RevSubClamp, 1);
	CodegenDrawWall("tmvline4_revsubclamp", DrawWallVariant::RevSubClamp, 4);

	mProgram.engine()->finalizeObject();
	mProgram.modulePassManager()->run(*mProgram.module());

	FillColumn = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("FillColumn");
	FillColumnAdd = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("FillColumnAdd");
	FillColumnAddClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("FillColumnAddClamp");
	FillColumnSubClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("FillColumnSubClamp");
	FillColumnRevSubClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("FillColumnRevSubClamp");
	DrawColumn = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumn");
	DrawColumnAdd = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnAdd");
	DrawColumnShaded = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnShaded");
	DrawColumnAddClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnAddClamp");
	DrawColumnSubClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnSubClamp");
	DrawColumnRevSubClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRevSubClamp");
	DrawColumnTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnTranslated");
	DrawColumnTlatedAdd = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnTlatedAdd");
	DrawColumnAddClampTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnAddClampTranslated");
	DrawColumnSubClampTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnSubClampTranslated");
	DrawColumnRevSubClampTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRevSubClampTranslated");
	DrawColumnRt1 = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1");
	DrawColumnRt1Copy = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1Copy");
	DrawColumnRt1Add = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1Add");
	DrawColumnRt1Shaded = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1Shaded");
	DrawColumnRt1AddClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1AddClamp");
	DrawColumnRt1SubClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1SubClamp");
	DrawColumnRt1RevSubClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1RevSubClamp");
	DrawColumnRt1Translated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1Translated");
	DrawColumnRt1TlatedAdd = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1TlatedAdd");
	DrawColumnRt1AddClampTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1AddClampTranslated");
	DrawColumnRt1SubClampTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1SubClampTranslated");
	DrawColumnRt1RevSubClampTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt1RevSubClampTranslated");
	DrawColumnRt4 = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4");
	DrawColumnRt4Copy = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4Copy");
	DrawColumnRt4Add = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4Add");
	DrawColumnRt4Shaded = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4Shaded");
	DrawColumnRt4AddClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4AddClamp");
	DrawColumnRt4SubClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4SubClamp");
	DrawColumnRt4RevSubClamp = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4RevSubClamp");
	DrawColumnRt4Translated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4Translated");
	DrawColumnRt4TlatedAdd = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4TlatedAdd");
	DrawColumnRt4AddClampTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4AddClampTranslated");
	DrawColumnRt4SubClampTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4SubClampTranslated");
	DrawColumnRt4RevSubClampTranslated = mProgram.GetProcAddress<void(const DrawColumnArgs *, const WorkerThreadData *)>("DrawColumnRt4RevSubClampTranslated");
	DrawSpan = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpan");
	DrawSpanMasked = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanMasked");
	DrawSpanTranslucent = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanTranslucent");
	DrawSpanMaskedTranslucent = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanMaskedTranslucent");
	DrawSpanAddClamp = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanAddClamp");
	DrawSpanMaskedAddClamp = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanMaskedAddClamp");
	vlinec1 = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("vlinec1");
	vlinec4 = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("vlinec4");
	mvlinec1 = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("mvlinec1");
	mvlinec4 = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("mvlinec4");
	tmvline1_add = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("tmvline1_add");
	tmvline4_add = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("tmvline4_add");
	tmvline1_addclamp = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("tmvline1_addclamp");
	tmvline4_addclamp = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("tmvline4_addclamp");
	tmvline1_subclamp = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("tmvline1_subclamp");
	tmvline4_subclamp = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("tmvline4_subclamp");
	tmvline1_revsubclamp = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("tmvline1_revsubclamp");
	tmvline4_revsubclamp = mProgram.GetProcAddress<void(const DrawWallArgs *, const WorkerThreadData *)>("tmvline4_revsubclamp");

	mProgram.StopLogFatalErrors();
}

void LLVMDrawersImpl::CodegenDrawColumn(const char *name, DrawColumnVariant variant, DrawColumnMethod method)
{
	llvm::IRBuilder<> builder(mProgram.context());
	SSAScope ssa_scope(&mProgram.context(), mProgram.module(), &builder);

	SSAFunction function(name);
	function.add_parameter(GetDrawColumnArgsStruct(mProgram.context()));
	function.add_parameter(GetWorkerThreadDataStruct(mProgram.context()));
	function.create_public();

	DrawColumnCodegen codegen;
	codegen.Generate(variant, method, function.parameter(0), function.parameter(1));

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		I_FatalError("verifyFunction failed for " __FUNCTION__);

	mProgram.functionPassManager()->run(*function.func);
}

void LLVMDrawersImpl::CodegenDrawSpan(const char *name, DrawSpanVariant variant)
{
	llvm::IRBuilder<> builder(mProgram.context());
	SSAScope ssa_scope(&mProgram.context(), mProgram.module(), &builder);

	SSAFunction function(name);
	function.add_parameter(GetDrawSpanArgsStruct(mProgram.context()));
	function.create_public();

	DrawSpanCodegen codegen;
	codegen.Generate(variant, function.parameter(0));

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		I_FatalError("verifyFunction failed for " __FUNCTION__);

	mProgram.functionPassManager()->run(*function.func);
}

void LLVMDrawersImpl::CodegenDrawWall(const char *name, DrawWallVariant variant, int columns)
{
	llvm::IRBuilder<> builder(mProgram.context());
	SSAScope ssa_scope(&mProgram.context(), mProgram.module(), &builder);

	SSAFunction function(name);
	function.add_parameter(GetDrawWallArgsStruct(mProgram.context()));
	function.add_parameter(GetWorkerThreadDataStruct(mProgram.context()));
	function.create_public();

	DrawWallCodegen codegen;
	codegen.Generate(variant, columns == 4, function.parameter(0), function.parameter(1));

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		I_FatalError("verifyFunction failed for " __FUNCTION__);

	mProgram.functionPassManager()->run(*function.func);
}

llvm::Type *LLVMDrawersImpl::GetDrawColumnArgsStruct(llvm::LLVMContext &context)
{
	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // uint32_t *dest;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // const uint32_t *source;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // uint8_t *colormap;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // uint8_t *translation;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // const uint32_t *basecolors;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t pitch;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t count;
	elements.push_back(llvm::Type::getInt32Ty(context)); // int32_t dest_y;
	elements.push_back(llvm::Type::getInt32Ty(context)); // uint32_t iscale;
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
	return llvm::StructType::get(context, elements, false)->getPointerTo();
}

llvm::Type *LLVMDrawersImpl::GetDrawSpanArgsStruct(llvm::LLVMContext &context)
{
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
	return llvm::StructType::get(context, elements, false)->getPointerTo();
}

llvm::Type *LLVMDrawersImpl::GetDrawWallArgsStruct(llvm::LLVMContext &context)
{
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
	return llvm::StructType::get(context, elements, false)->getPointerTo();
}

llvm::Type *LLVMDrawersImpl::GetWorkerThreadDataStruct(llvm::LLVMContext &context)
{
	std::vector<llvm::Type *> elements;
	for (int i = 0; i < 4; i++)
		elements.push_back(llvm::Type::getInt32Ty(context));
	elements.push_back(llvm::Type::getInt8PtrTy(context));
	return llvm::StructType::get(context, elements, false)->getPointerTo();
}

/////////////////////////////////////////////////////////////////////////////

namespace { static bool LogFatalErrors = false; }

LLVMProgram::LLVMProgram()
{
	using namespace llvm;

	// We have to extra careful about this because both LLVM and ZDoom made
	// the very unwise decision to hook atexit. To top it off, LLVM decided
	// to log something in the atexit handler..
	LogFatalErrors = true;

	install_fatal_error_handler([](void *user_data, const std::string& reason, bool gen_crash_diag) {
		if (LogFatalErrors)
			I_FatalError("LLVM fatal error: %s", reason.c_str());
	});

	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();

	std::string errorstring;

	std::string targetTriple = sys::getProcessTriple();
	std::string cpuName = sys::getHostCPUName();
	StringMap<bool> cpuFeatures;
	sys::getHostCPUFeatures(cpuFeatures);
	std::string cpuFeaturesStr;
	for (const auto &it : cpuFeatures)
	{
		if (!cpuFeaturesStr.empty())
			cpuFeaturesStr.push_back(' ');
		cpuFeaturesStr.push_back(it.getValue() ? '+' : '-');
		cpuFeaturesStr += it.getKey();
	}

	DPrintf(DMSG_SPAMMY, "LLVM target triple: %s\n", targetTriple.c_str());
	DPrintf(DMSG_SPAMMY, "LLVM CPU and features: %s, %s\n", cpuName.c_str(), cpuFeaturesStr.c_str());

	const Target *target = TargetRegistry::lookupTarget(targetTriple, errorstring);
	if (!target)
		I_FatalError("Could not find LLVM target: %s", errorstring.c_str());

	TargetOptions opt;
	auto relocModel = Optional<Reloc::Model>(Reloc::Static);
	TargetMachine *machine = target->createTargetMachine(targetTriple, cpuName, cpuFeaturesStr, opt, relocModel, CodeModel::Default, CodeGenOpt::Aggressive);
	if (!machine)
		I_FatalError("Could not create LLVM target machine");

	mContext = std::make_unique<LLVMContext>();

	auto moduleOwner = std::make_unique<Module>("render", context());
	mModule = moduleOwner.get();
	mModule->setTargetTriple(targetTriple);
	mModule->setDataLayout(machine->createDataLayout());

	EngineBuilder engineBuilder(std::move(moduleOwner));
	engineBuilder.setErrorStr(&errorstring);
	engineBuilder.setOptLevel(CodeGenOpt::Aggressive);
	engineBuilder.setRelocationModel(Reloc::Static);
	engineBuilder.setEngineKind(EngineKind::JIT);
	mEngine.reset(engineBuilder.create(machine));
	if (!mEngine)
		I_FatalError("Could not create LLVM execution engine: %s", errorstring.c_str());

	mModulePassManager = std::make_unique<legacy::PassManager>();
	mFunctionPassManager = std::make_unique<legacy::FunctionPassManager>(mModule);

	PassManagerBuilder passManagerBuilder;
	passManagerBuilder.OptLevel = 3;
	passManagerBuilder.SizeLevel = 0;
	passManagerBuilder.Inliner = createFunctionInliningPass();
	passManagerBuilder.populateModulePassManager(*mModulePassManager.get());
	passManagerBuilder.populateFunctionPassManager(*mFunctionPassManager.get());
}

LLVMProgram::~LLVMProgram()
{
	mEngine.reset();
	mContext.reset();
}

void *LLVMProgram::PointerToFunction(const char *name)
{
	llvm::Function *function = mModule->getFunction(name);
	if (!function)
		return nullptr;
	return mEngine->getPointerToFunction(function);
}

void LLVMProgram::StopLogFatalErrors()
{
	LogFatalErrors = false;
}
