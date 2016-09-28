
#pragma once

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
#include "r_compiler/ssa/ssa_pixels.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_barycentric_weight.h"
#include "r_compiler/llvm_include.h"

class RenderProgram
{
public:
	RenderProgram();
	~RenderProgram();

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

struct RenderArgs
{
	uint32_t *destorg;
	const uint32_t *source;
	int32_t destpitch;
	int32_t xfrac;
	int32_t yfrac;
	int32_t xstep;
	int32_t ystep;
	int32_t x1;
	int32_t x2;
	int32_t y;
	int32_t xbits;
	int32_t ybits;
	uint32_t light;
	uint32_t srcalpha;
	uint32_t destalpha;

	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	uint32_t flags;
	enum Flags
	{
		simple_shade = 1,
		nearest_filter = 2
	};
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

	// SampleBgra
	SSAVec4i sample_linear(SSAUBytePtr col0, SSAUBytePtr col1, SSAInt texturefracx, SSAInt texturefracy, SSAInt one, SSAInt height);
	SSAVec4i sample_linear(SSAUBytePtr texture, SSAInt xfrac, SSAInt yfrac, SSAInt xbits, SSAInt ybits);
};

class DrawSpanCodegen : public DrawerCodegen
{
public:
	void Generate(SSAValue args);

private:
	void LoopShade(bool isSimpleShade);
	void LoopFilter(bool isSimpleShade, bool isNearestFilter);
	SSAInt Loop4x(bool isSimpleShade, bool isNearestFilter, bool is64x64);
	void Loop(SSAInt start, bool isSimpleShade, bool isNearestFilter, bool is64x64);
	SSAVec4i Sample(SSAInt xfrac, SSAInt yfrac, bool isNearestFilter, bool is64x64);

	SSAStack<SSAInt> stack_index, stack_xfrac, stack_yfrac;

	SSAUBytePtr destorg;
	SSAUBytePtr source;
	SSAInt destpitch;
	SSAInt xstep;
	SSAInt ystep;
	SSAInt x1;
	SSAInt x2;
	SSAInt y;
	SSAInt xbits;
	SSAInt ybits;
	SSAInt light;
	SSAInt srcalpha;
	SSAInt destalpha;
	SSAInt count;
	SSAUBytePtr data;
	SSAInt yshift;
	SSAInt xshift;
	SSAInt xmask;
	SSABool is_64x64;
	SSABool is_simple_shade;
	SSABool is_nearest_filter;
	SSAShadeConstants shade_constants;
};

class FixedFunction
{
public:
	FixedFunction();

	void(*DrawSpan)(const RenderArgs *) = nullptr;

private:
	void CodegenDrawSpan();

	static llvm::Type *GetRenderArgsStruct(llvm::LLVMContext &context);

	RenderProgram mProgram;
};
