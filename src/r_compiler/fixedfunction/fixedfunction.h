
#pragma once

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

class FixedFunction
{
public:
	FixedFunction();

	void(*DrawSpan)(const RenderArgs *) = nullptr;

private:
	void CodegenDrawSpan();

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

	static llvm::Type *GetRenderArgsStruct(llvm::LLVMContext &context);

	RenderProgram mProgram;
};

#if 0

class GlslProgram;
class GlslCodeGen;

class GlslFixedFunction
{
public:
	GlslFixedFunction(GlslProgram &program, GlslCodeGen &vertex_codegen, GlslCodeGen &fragment_codegen);
	void codegen();
	static llvm::Type *get_sampler_struct(llvm::LLVMContext &context);

private:
	void codegen_draw_triangles(int num_vertex_in, int num_vertex_out);
	void codegen_calc_window_positions();
	void codegen_calc_polygon_face_direction();
	void codegen_calc_polygon_y_range();
	void codegen_update_polygon_edge();
	void codegen_texture();
	void codegen_normalize();
	void codegen_reflect();
	void codegen_max();
	void codegen_pow();
	void codegen_dot();
	void codegen_mix();

	struct OuterData
	{
		OuterData() : sampler() { }

		SSAInt start;
		SSAInt end;
		SSAInt input_width;
		SSAInt input_height;
		SSAInt output_width;
		SSAInt output_height;
		SSAUBytePtr input_pixels;
		SSAUBytePtr output_pixels_line;

		SSAVec4fPtr sse_left_varying_in;
		SSAVec4fPtr sse_right_varying_in;
		int num_varyings;
		SSAVec4f viewport_x;
		SSAVec4f viewport_rcp_half_width;
		SSAVec4f dx;
		SSAVec4f dw;
		SSAVec4f v1w;
		SSAVec4f v1x;

		llvm::Value *sampler;
	};

	void render_polygon(
		SSAInt input_width,
		SSAInt input_height,
		SSAUBytePtr input_data,
		SSAInt output_width,
		SSAInt output_height,
		SSAUBytePtr output_data,
		SSAInt viewport_x,
		SSAInt viewport_y,
		SSAInt viewport_width,
		SSAInt viewport_height,
		SSAInt num_vertices,
		std::vector<SSAVec4fPtr> fragment_ins,
		SSAInt core,
		SSAInt num_cores);

	void codegen_render_scanline(int num_varyings);
	void process_first_pixels(OuterData &outer_data, SSAStack<SSAInt> &stack_x, SSAStack<SSAVec4f> &stack_xnormalized);
	void process_last_pixels(OuterData &outer_data, SSAStack<SSAInt> &stack_x, SSAStack<SSAVec4f> &stack_xnormalized);
	void inner_block(OuterData &data, SSAVec4f xnormalized, SSAVec4f *out_frag_colors);
	void blend(SSAVec4f frag_colors[4], SSAVec16ub &dest);

	GlslProgram &program;
	GlslCodeGen &vertex_codegen;
	GlslCodeGen &fragment_codegen;
};

#endif
