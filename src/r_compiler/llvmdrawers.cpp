
#include "i_system.h"
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
	void CodegenDrawSpan(const char *name, DrawSpanVariant variant);
	void CodegenDrawWall(const char *name, DrawWallVariant variant, int columns);

	static llvm::Type *GetDrawSpanArgsStruct(llvm::LLVMContext &context);
	static llvm::Type *GetDrawWallArgsStruct(llvm::LLVMContext &context);

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

	DrawSpan = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpan");
	DrawSpanMasked = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanMasked");
	DrawSpanTranslucent = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanTranslucent");
	DrawSpanMaskedTranslucent = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanMaskedTranslucent");
	DrawSpanAddClamp = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanAddClamp");
	DrawSpanMaskedAddClamp = mProgram.GetProcAddress<void(const DrawSpanArgs *)>("DrawSpanMaskedAddClamp");
	vlinec1 = mProgram.GetProcAddress<void(const DrawWallArgs *)>("vlinec1");
	vlinec4 = mProgram.GetProcAddress<void(const DrawWallArgs *)>("vlinec4");
	mvlinec1 = mProgram.GetProcAddress<void(const DrawWallArgs *)>("mvlinec1");
	mvlinec4 = mProgram.GetProcAddress<void(const DrawWallArgs *)>("mvlinec4");
	tmvline1_add = mProgram.GetProcAddress<void(const DrawWallArgs *)>("tmvline1_add");
	tmvline4_add = mProgram.GetProcAddress<void(const DrawWallArgs *)>("tmvline4_add");
	tmvline1_addclamp = mProgram.GetProcAddress<void(const DrawWallArgs *)>("tmvline1_addclamp");
	tmvline4_addclamp = mProgram.GetProcAddress<void(const DrawWallArgs *)>("tmvline4_addclamp");
	tmvline1_subclamp = mProgram.GetProcAddress<void(const DrawWallArgs *)>("tmvline1_subclamp");
	tmvline4_subclamp = mProgram.GetProcAddress<void(const DrawWallArgs *)>("tmvline4_subclamp");
	tmvline1_revsubclamp = mProgram.GetProcAddress<void(const DrawWallArgs *)>("tmvline1_revsubclamp");
	tmvline4_revsubclamp = mProgram.GetProcAddress<void(const DrawWallArgs *)>("tmvline4_revsubclamp");

	mProgram.StopLogFatalErrors();
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
	function.create_public();

	DrawWallCodegen codegen;
	codegen.Generate(variant, columns == 4, function.parameter(0));

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		I_FatalError("verifyFunction failed for " __FUNCTION__);

	mProgram.functionPassManager()->run(*function.func);
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
	InitializeNativeTargetAsmParser();

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

	//Printf("LLVM target triple: %s\n", targetTriple.c_str());
	//Printf("LLVM CPU and features: %s, %s\n", cpuName.c_str(), cpuFeaturesStr.c_str());

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
