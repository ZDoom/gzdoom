
#include "i_system.h"
#include "r_compiler/fixedfunction/fixedfunction.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"
#include "r_compiler/ssa/ssa_barycentric_weight.h"

RenderProgram::RenderProgram()
{
	using namespace llvm;

	install_fatal_error_handler([](void *user_data, const std::string& reason, bool gen_crash_diag) {
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

RenderProgram::~RenderProgram()
{
	mEngine.reset();
	mContext.reset();
}

void *RenderProgram::PointerToFunction(const char *name)
{
	llvm::Function *function = mModule->getFunction(name);
	if (!function)
		return nullptr;
	return mEngine->getPointerToFunction(function);
}

FixedFunction::FixedFunction()
{
	CodegenDrawSpan();
	mProgram.engine()->finalizeObject();
	mProgram.modulePassManager()->run(*mProgram.module());

	DrawSpan = mProgram.GetProcAddress<void(const RenderArgs *)>("DrawSpan");
}

void FixedFunction::CodegenDrawSpan()
{
	llvm::IRBuilder<> builder(mProgram.context());
	SSAScope ssa_scope(&mProgram.context(), mProgram.module(), &builder);

	SSAFunction function("DrawSpan");
	function.add_parameter(GetRenderArgsStruct(mProgram.context()));
	function.create_public();

	SSAStack<SSAInt> stack_index, stack_xfrac, stack_yfrac;

	SSAValue args = function.parameter(0);
	SSAUBytePtr destorg = args[0][0].load();
	SSAUBytePtr source = args[0][1].load();
	SSAInt destpitch = args[0][2].load();
	stack_xfrac.store(args[0][3].load());
	stack_yfrac.store(args[0][4].load());
	SSAInt xstep = args[0][5].load();
	SSAInt ystep = args[0][6].load();
	SSAInt x1 = args[0][7].load();
	SSAInt x2 = args[0][8].load();
	SSAInt y = args[0][9].load();
	SSAInt xbits = args[0][10].load();
	SSAInt ybits = args[0][11].load();
	SSAInt light = args[0][12].load();
	SSAInt srcalpha = args[0][13].load();
	SSAInt destalpha = args[0][14].load();

	SSAInt count = x2 - x1 + 1;
	SSAUBytePtr data = destorg[(x1 + y * destpitch) * 4];

	SSAInt yshift = 32 - ybits;
	SSAInt xshift = yshift - xbits;
	SSAInt xmask = ((SSAInt(1) << xbits) - 1) << ybits;
	//is_64x64 = xbits == 6 && ybits == 6;

	SSAInt sseLength = count / 4;
	stack_index.store(0);
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < sseLength);

		SSAVec4i colors[4];
		for (int i = 0; i < 4; i++)
		{
			SSAInt xfrac = stack_xfrac.load();
			SSAInt yfrac = stack_yfrac.load();

			// 64x64 is the most common case by far, so special case it.
			SSAInt spot64 = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
			//SSAInt spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);

			colors[i] = shade_bgra_simple(source[spot64 * 4].load_vec4ub(), light);

			stack_xfrac.store(xfrac + xstep);
			stack_yfrac.store(yfrac + ystep);
		}

		SSAVec16ub ssecolors(SSAVec8s(colors[0], colors[1]), SSAVec8s(colors[2], colors[3]));
		data[index * 16].store_unaligned_vec16ub(ssecolors);

		stack_index.store(index + 1);
		loop.end_block();
	}

	stack_index.store(sseLength * 4);
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < count);

		SSAInt xfrac = stack_xfrac.load();
		SSAInt yfrac = stack_yfrac.load();

		// 64x64 is the most common case by far, so special case it.
		SSAInt spot64 = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
		//SSAInt spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);

		SSAVec4i color = shade_bgra_simple(source[spot64 * 4].load_vec4ub(), light);
		data[index * 4].store_vec4ub(color);

		stack_index.store(index + 1);
		stack_xfrac.store(xfrac + xstep);
		stack_yfrac.store(yfrac + ystep);
		loop.end_block();
	}

	builder.CreateRetVoid();

	if (llvm::verifyFunction(*function.func))
		I_FatalError("verifyFunction failed for " __FUNCTION__);

	mProgram.functionPassManager()->run(*function.func);
}

SSAInt FixedFunction::calc_light_multiplier(SSAInt light)
{
	return 256 - (light >> (FRACBITS - 8));
}

SSAVec4i FixedFunction::shade_pal_index_simple(SSAInt index, SSAInt light, SSAUBytePtr basecolors)
{
	SSAVec4i color = basecolors[index * 4].load_vec4ub(); // = GPalette.BaseColors[index];
	return shade_bgra_simple(color, light);
}

SSAVec4i FixedFunction::shade_pal_index_advanced(SSAInt index, SSAInt light, const SSAShadeConstants &constants, SSAUBytePtr basecolors)
{
	SSAVec4i color = basecolors[index * 4].load_vec4ub(); // = GPalette.BaseColors[index];
	return shade_bgra_advanced(color, light, constants);
}

SSAVec4i FixedFunction::shade_bgra_simple(SSAVec4i color, SSAInt light)
{
	color = color * light / 256;
	return color.insert(3, 255);
}

SSAVec4i FixedFunction::shade_bgra_advanced(SSAVec4i color, SSAInt light, const SSAShadeConstants &constants)
{
	SSAInt blue = color[0];
	SSAInt green = color[1];
	SSAInt red = color[2];
	SSAInt alpha = color[3];

	SSAInt intensity = ((red * 77 + green * 143 + blue * 37) >> 8) * constants.desaturate;

	SSAVec4i inv_light = 256 - light;
	SSAVec4i inv_desaturate = 256 - constants.desaturate;

	color = (color * inv_desaturate + intensity) / 256;
	color = (constants.fade * inv_light + color * light) / 256;
	color = (color * constants.light) / 256;

	return color.insert(3, alpha);
}

SSAVec4i FixedFunction::blend_copy(SSAVec4i fg)
{
	return fg;
}

SSAVec4i FixedFunction::blend_add(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha)
{
	SSAVec4i color = (fg * srcalpha + bg * destalpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i FixedFunction::blend_sub(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha)
{
	SSAVec4i color = (bg * destalpha - fg * srcalpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i FixedFunction::blend_revsub(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha)
{
	SSAVec4i color = (fg * srcalpha - bg * destalpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i FixedFunction::blend_alpha_blend(SSAVec4i fg, SSAVec4i bg)
{
	SSAInt alpha = fg[3];
	alpha = alpha + (alpha >> 7); // // 255 -> 256
	SSAInt inv_alpha = 256 - alpha;
	SSAVec4i color = (fg * alpha + bg * inv_alpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i FixedFunction::sample_linear(SSAUBytePtr col0, SSAUBytePtr col1, SSAInt texturefracx, SSAInt texturefracy, SSAInt one, SSAInt height)
{
	SSAInt frac_y0 = (texturefracy >> FRACBITS) * height;
	SSAInt frac_y1 = ((texturefracy + one) >> FRACBITS) * height;
	SSAInt y0 = frac_y0 >> FRACBITS;
	SSAInt y1 = frac_y1 >> FRACBITS;

	SSAVec4i p00 = col0[y0].load_vec4ub();
	SSAVec4i p01 = col0[y1].load_vec4ub();
	SSAVec4i p10 = col1[y0].load_vec4ub();
	SSAVec4i p11 = col1[y1].load_vec4ub();

	SSAInt inv_b = texturefracx;
	SSAInt inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
	SSAInt a = 16 - inv_a;
	SSAInt b = 16 - inv_b;

	return (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
}

SSAVec4i FixedFunction::sample_linear(SSAUBytePtr texture, SSAInt xfrac, SSAInt yfrac, SSAInt xbits, SSAInt ybits)
{
	SSAInt xshift = (32 - xbits);
	SSAInt yshift = (32 - ybits);
	SSAInt xmask = (SSAInt(1) << xshift) - 1;
	SSAInt ymask = (SSAInt(1) << yshift) - 1;
	SSAInt x = xfrac >> xbits;
	SSAInt y = yfrac >> ybits;

	SSAVec4i p00 = texture[(y & ymask) + ((x & xmask) << yshift)].load_vec4ub();
	SSAVec4i p01 = texture[((y + 1) & ymask) + ((x & xmask) << yshift)].load_vec4ub();
	SSAVec4i p10 = texture[(y & ymask) + (((x + 1) & xmask) << yshift)].load_vec4ub();
	SSAVec4i p11 = texture[((y + 1) & ymask) + (((x + 1) & xmask) << yshift)].load_vec4ub();

	SSAInt inv_b = (xfrac >> (xbits - 4)) & 15;
	SSAInt inv_a = (yfrac >> (ybits - 4)) & 15;
	SSAInt a = 16 - inv_a;
	SSAInt b = 16 - inv_b;

	return (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
}

llvm::Type *FixedFunction::GetRenderArgsStruct(llvm::LLVMContext &context)
{
	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // uint8_t *destorg;
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // const uint8_t *source;
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
	return llvm::StructType::get(context, elements, false)->getPointerTo();
}

#if 0

GlslFixedFunction::GlslFixedFunction(GlslProgram &program, GlslCodeGen &vertex_codegen, GlslCodeGen &fragment_codegen)
: program(program), vertex_codegen(vertex_codegen), fragment_codegen(fragment_codegen)
{
}

llvm::Type *GlslFixedFunction::get_sampler_struct(llvm::LLVMContext &context)
{
	std::vector<llvm::Type *> elements;
	elements.push_back(llvm::Type::getInt32Ty(context)); // width
	elements.push_back(llvm::Type::getInt32Ty(context)); // height
	elements.push_back(llvm::Type::getInt8PtrTy(context)); // data
	return llvm::StructType::get(context, elements, false);
}

void GlslFixedFunction::codegen()
{
	codegen_render_scanline(5);
	codegen_calc_window_positions();
	codegen_calc_polygon_face_direction();
	codegen_calc_polygon_y_range();
	codegen_update_polygon_edge();
	codegen_draw_triangles(5, 5);
	codegen_texture();
	codegen_normalize();
	codegen_reflect();
	codegen_max();
	codegen_pow();
	codegen_dot();
	codegen_mix();
}

void GlslFixedFunction::codegen_texture()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("fragment_texture");
	function.add_parameter(fragment_codegen.get_global_struct_type());
	function.add_parameter(get_sampler_struct(program.context()));
	function.add_parameter(SSAVec4f::llvm_type());
	function.create_private();

	SSAValue sampler_ptr = function.parameter(1);
	SSAVec4f pos = function.parameter(2);

	SSAInt width = sampler_ptr[0][0].load();
	SSAInt height = sampler_ptr[0][1].load();
	SSAUBytePtr data = sampler_ptr[0][2].load();

	SSAPixels4ub_argb_rev pixels(width, height, data);
	//builder.CreateRet(pixels.linear_clamp4f(pos).v);
	builder.CreateRet(pixels.linear_clamp4f(pos[0], pos[1]).v);

	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_normalize()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("fragment_normalize");
	function.add_parameter(fragment_codegen.get_global_struct_type());
	function.add_parameter(SSAVec4f::llvm_type());
	function.create_private();

	SSAVec4f vec = function.parameter(1);

	// To do: this can probably be done a lot faster with _mm_rsqrt_ss
	SSAVec4f vec2 = vec * vec;
	SSAVec4f length3(SSAFloat::sqrt(vec2[0] + vec2[1] + vec2[2]));
	SSAVec4f normalized = vec / length3;
	builder.CreateRet(normalized.v);

	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_reflect()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("fragment_reflect");
	function.add_parameter(fragment_codegen.get_global_struct_type());
	function.add_parameter(SSAVec4f::llvm_type());
	function.add_parameter(SSAVec4f::llvm_type());
	function.create_private();

	SSAVec4f i = function.parameter(1);
	SSAVec4f n = function.parameter(2);

	SSAVec4f c = i * n;
	SSAFloat dot3 = c[0] + c[1] + c[2];
	SSAVec4f result = i - (2.0f * dot3) * n;
	builder.CreateRet(result.v);

	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_max()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("fragment_max");
	function.add_parameter(fragment_codegen.get_global_struct_type());
	function.add_parameter(SSAFloat::llvm_type());
	function.add_parameter(SSAFloat::llvm_type());
	function.create_private();

	SSAFloat a = function.parameter(1);
	SSAFloat b = function.parameter(2);

	SSAPhi<SSAFloat> phi;
	SSAIfBlock branch;
	branch.if_block(a >= b);
		phi.add_incoming(a);
	branch.else_block();
		phi.add_incoming(b);
	branch.end_block();
	SSAFloat c = phi.create();

	builder.CreateRet(c.v);
	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_pow()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("fragment_pow");
	function.add_parameter(fragment_codegen.get_global_struct_type());
	function.add_parameter(SSAFloat::llvm_type());
	function.add_parameter(SSAFloat::llvm_type());
	function.create_private();

	SSAFloat a = function.parameter(1);
	SSAFloat b = function.parameter(2);
	builder.CreateRet(a.v);
	//builder.CreateRet(SSAFloat::pow(a, b).v);

	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_dot()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("fragment_dot");
	function.add_parameter(fragment_codegen.get_global_struct_type());
	function.add_parameter(SSAVec4f::llvm_type());
	function.add_parameter(SSAVec4f::llvm_type());
	function.create_private();

	SSAVec4f a = function.parameter(1);
	SSAVec4f b = function.parameter(2);

	SSAVec4f c = a * b;
	SSAFloat dot3 = c[0] + c[1] + c[2];
	builder.CreateRet(dot3.v);

	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_mix()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("fragment_mix");
	function.add_parameter(fragment_codegen.get_global_struct_type());
	function.add_parameter(SSAVec4f::llvm_type());
	function.add_parameter(SSAVec4f::llvm_type());
	function.add_parameter(SSAFloat::llvm_type());
	function.create_private();

	SSAVec4f v1 = function.parameter(1);
	SSAVec4f v2 = function.parameter(2);
	SSAFloat t = function.parameter(3);

	SSAVec4f b = t;
	SSAVec4f a = 1.0f - b;
	SSAVec4f mix = v1 * a + v2 * b;
	builder.CreateRet(mix.v);

	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_draw_triangles(int num_vertex_in, int num_vertex_out)
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("draw_triangles");
	function.add_parameter(SSAInt::llvm_type()); // input_width
	function.add_parameter(SSAInt::llvm_type()); // input_height
	function.add_parameter(SSAUBytePtr::llvm_type()); // input_data
	function.add_parameter(SSAInt::llvm_type()); // output_width
	function.add_parameter(SSAInt::llvm_type()); // output_height
	function.add_parameter(SSAUBytePtr::llvm_type()); // output_data
	function.add_parameter(SSAInt::llvm_type()); // viewport_x
	function.add_parameter(SSAInt::llvm_type()); // viewport_y
	function.add_parameter(SSAInt::llvm_type()); // viewport_width
	function.add_parameter(SSAInt::llvm_type()); // viewport_height
	function.add_parameter(SSAVec4fPtr::llvm_type()); // uniforms
	function.add_parameter(SSAInt::llvm_type()); // first_vertex
	function.add_parameter(SSAInt::llvm_type()); // num_vertices
	function.add_parameter(SSAVec4fPtr::llvm_type()->getPointerTo()); // vertex attributes
	function.add_parameter(SSAInt::llvm_type()); // core
	function.add_parameter(SSAInt::llvm_type()); // num_cores
	function.create_public();

	SSAInt input_width = function.parameter(0);
	SSAInt input_height = function.parameter(1);
	SSAUBytePtr input_data = function.parameter(2);
	SSAInt output_width = function.parameter(3);
	SSAInt output_height = function.parameter(4);
	SSAUBytePtr output_data = function.parameter(5);
	SSAInt viewport_x = function.parameter(6);
	SSAInt viewport_y = function.parameter(7);
	SSAInt viewport_width = function.parameter(8);
	SSAInt viewport_height = function.parameter(9);
	SSAVec4fPtr uniforms = function.parameter(10);
	SSAInt first_vertex = function.parameter(11);
	SSAInt num_vertices = function.parameter(12);
	SSAValue vertex_in_ptr = function.parameter(13);
	SSAInt core = function.parameter(14);
	SSAInt num_cores = function.parameter(15);

	SSAStack<SSAInt> stack_vertex_index;
	SSAValue vertex_globals_ptr = SSAValue::from_llvm(SSAScope::alloca(vertex_codegen.get_global_struct_type()));
	std::vector<SSAVec4fPtr> vertex_outs;
	for (int i = 0; i < num_vertex_out; i++)
		vertex_outs.push_back(SSAVec4fPtr::from_llvm(SSAScope::builder().CreateAlloca(SSAVec4f::llvm_type(), SSAInt(3).v)));

	int num_uniforms = 1;
	{
		llvm::Type *type = llvm::ArrayType::get(llvm::VectorType::get(llvm::Type::getFloatTy(program.context()), 4), 4);
		llvm::Value *matrix = llvm::UndefValue::get(type);
		for (int col = 0; col < 4; col++)
		{
			SSAVec4f column = uniforms[col].load_unaligned();
			std::vector<unsigned int> indexes;
			indexes.push_back(col);
			matrix = builder.CreateInsertValue(matrix, column.v, indexes);
		}
		vertex_globals_ptr[0][0].store(matrix);
	}

	stack_vertex_index.store(0);
	SSAForBlock loop;
	SSAInt vertex_index = stack_vertex_index.load();
	loop.loop_block(vertex_index + 2 < num_vertices);
	for (int v = 0; v < 3; v++)
	{
		for (int i = 0; i < num_vertex_in; i++)
		{
			SSAValue attribute_ptr = vertex_in_ptr[i].load();
			SSAVec4f vertex_in = SSAVec4f::shuffle(SSAVec4fPtr(attribute_ptr)[first_vertex + vertex_index + v].load_unaligned(), 0, 1, 2, 3);
			vertex_globals_ptr[0][num_uniforms + i].store(vertex_in.v);
		}
		SSAScope::builder().CreateCall(SSAScope::module()->getFunction((vertex_codegen.shader_prefix() + "main").c_str()), vertex_globals_ptr.v);
		for (int i = 0; i < num_vertex_out; i++)
		{
			vertex_outs[i][v].store(vertex_globals_ptr[0][num_uniforms + num_vertex_in + i].load());
		}
	}

	render_polygon(input_width, input_height, input_data, output_width, output_height, output_data, viewport_x, viewport_y, viewport_width, viewport_height, 3, vertex_outs, core, num_cores);

	stack_vertex_index.store(vertex_index + 3);
	loop.end_block();

	builder.CreateRetVoid();
	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_calc_window_positions()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("calc_window_positions");
	function.add_parameter(SSAInt::llvm_type()); // viewport_x
	function.add_parameter(SSAInt::llvm_type()); // viewport_y
	function.add_parameter(SSAInt::llvm_type()); // viewport_width
	function.add_parameter(SSAInt::llvm_type()); // viewport_height
	function.add_parameter(SSAInt::llvm_type()); // num_vertices
	function.add_parameter(SSAVec4fPtr::llvm_type()); // gl_Position
	function.add_parameter(SSAVec4fPtr::llvm_type()); // window_pos
	function.create_private();
	SSAInt viewport_x = function.parameter(0);
	SSAInt viewport_y = function.parameter(1);
	SSAInt viewport_width = function.parameter(2);
	SSAInt viewport_height = function.parameter(3);
	SSAInt num_vertices = function.parameter(4);
	SSAVec4fPtr clip_positions = function.parameter(5);
	SSAVec4fPtr window_positions = function.parameter(6);

	SSAViewport viewport(viewport_x, viewport_y, viewport_width, viewport_height);
	SSAStack<SSAInt> stack_transform_index;
	stack_transform_index.store(0);
	SSAForBlock loop_transform;
	SSAInt transform_index = stack_transform_index.load();
	loop_transform.loop_block(transform_index < num_vertices);
	{
		SSAVec4f clip_pos = clip_positions[transform_index].load();
		SSAVec4f window_pos = viewport.clip_to_window(clip_pos);
		window_positions[transform_index].store(window_pos);

		stack_transform_index.store(transform_index + 1);
	}
	loop_transform.end_block();

	builder.CreateRetVoid();
	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_calc_polygon_face_direction()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("calc_polygon_face_direction");
	function.set_return_type(SSABool::llvm_type());
	function.add_parameter(SSAInt::llvm_type()); // num_vertices
	function.add_parameter(SSAVec4fPtr::llvm_type()); // window_pos
	function.create_private();
	SSAInt num_vertices = function.parameter(0);
	SSAVec4fPtr window_positions = function.parameter(1);

	SSAStack<SSAFloat> stack_face_direction;
	SSAStack<SSAInt> stack_face_vertex_index;
	stack_face_direction.store(0.0f);
	stack_face_vertex_index.store(0);
	SSAForBlock loop_face_direction;
	SSAInt face_vertex_index = stack_face_vertex_index.load();
	loop_face_direction.loop_block(face_vertex_index < num_vertices);
	{
		SSAVec4f v0 = window_positions[face_vertex_index].load();
		SSAVec4f v1 = window_positions[(face_vertex_index + 1) % num_vertices].load();
		stack_face_direction.store(stack_face_direction.load() + v0[0] * v1[1] - v1[0] * v0[1]);
		stack_face_vertex_index.store(face_vertex_index + 1);
	}
	loop_face_direction.end_block();
	SSABool front_facing_ccw = (stack_face_direction.load() >= 0.0f);

	builder.CreateRet(front_facing_ccw.v);
	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_calc_polygon_y_range()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("calc_polygon_y_range");
	function.add_parameter(SSAInt::llvm_type()); // viewport_y
	function.add_parameter(SSAInt::llvm_type()); // viewport_height
	function.add_parameter(SSAInt::llvm_type()); // num_vertices
	function.add_parameter(SSAVec4fPtr::llvm_type()); // window_pos
	function.add_parameter(SSAInt::llvm_type()->getPointerTo()); // out_y_start
	function.add_parameter(SSAInt::llvm_type()->getPointerTo()); // out_y_end
	function.create_private();
	SSAInt viewport_y = function.parameter(0);
	SSAInt viewport_height = function.parameter(1);
	SSAInt num_vertices = function.parameter(2);
	SSAVec4fPtr window_positions = function.parameter(3);
	SSAValue out_y_start = function.parameter(4);
	SSAValue out_y_end = function.parameter(5);

	SSAStack<SSAInt> y_start;
	SSAStack<SSAInt> y_end;
	y_start.store(0x7fffffff);
	y_end.store(0);

	SSAStack<SSAInt> stack_minmax_index;
	stack_minmax_index.store(0);
	SSAForBlock loop_minmax;
	SSAInt minmax_index = stack_minmax_index.load();
	loop_minmax.loop_block(minmax_index < num_vertices);
	{
		SSAInt y = SSAInt(window_positions[minmax_index].load()[1] + 0.5f);
		y_start.store(ssa_min(y_start.load(), y));
		y_end.store(ssa_max(y_end.load(), y));
		stack_minmax_index.store(minmax_index + 1);
	}
	loop_minmax.end_block();

	y_start.store(ssa_max(y_start.load(), viewport_y));
	y_end.store(ssa_min(y_end.load(), viewport_y + viewport_height));

	out_y_start.store(y_start.load().v);
	out_y_end.store(y_end.load().v);
	builder.CreateRetVoid();
	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::codegen_update_polygon_edge()
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("update_polygon_edge");
	function.add_parameter(SSAFloat::llvm_type()); // y_position
	function.add_parameter(SSAInt::llvm_type()); // num_vertices
	function.add_parameter(SSAVec4fPtr::llvm_type()); // window_pos
	function.add_parameter(SSAInt::llvm_type()->getPointerTo()); // inout left_index
	function.add_parameter(SSAInt::llvm_type()->getPointerTo()); // inout right_index
	function.create_private();
	SSAFloat float_y = function.parameter(0);
	SSAInt num_vertices = function.parameter(1);
	SSAVec4fPtr window_positions = function.parameter(2);
	SSAValue ptr_left_index = function.parameter(3);
	SSAValue ptr_right_index = function.parameter(4);

	SSAStack<SSAInt> max_iterate;
	max_iterate.store(num_vertices);
	SSAForBlock loop_left;
	SSAInt left_index = ptr_left_index.load();
	SSAInt right_index = ptr_right_index.load();
	SSAInt next_left_index = (left_index + 1) % num_vertices;
	SSAFloat left_y0 = window_positions[left_index].load()[1];
	SSAFloat left_y1 = window_positions[next_left_index].load()[1];
	SSABool in_range = (left_y0 >= float_y && left_y1 < float_y) || (left_y1 >= float_y && left_y0 < float_y);
	loop_left.loop_block((left_index == right_index || !in_range) && max_iterate.load() > 0);
	ptr_left_index.store(next_left_index.v);
	max_iterate.store(max_iterate.load() - 1);
	loop_left.end_block();

	builder.CreateRetVoid();
	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::render_polygon(
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
	SSAInt num_cores)
{
	SSAVec4fPtr window_positions = SSAVec4fPtr::from_llvm(SSAScope::alloca(SSAVec4f::llvm_type(), num_vertices));
	SSAVec4fPtr left_line_varyings = SSAVec4fPtr::from_llvm(SSAScope::alloca(SSAVec4f::llvm_type(), fragment_ins.size()));
	SSAVec4fPtr right_line_varyings = SSAVec4fPtr::from_llvm(SSAScope::alloca(SSAVec4f::llvm_type(), fragment_ins.size()));

	///////////////////////////////////

	llvm::Value *calc_window_positions_args[] = { viewport_x.v, viewport_y.v, viewport_width.v, viewport_height.v, num_vertices.v, fragment_ins[0].v, window_positions.v };
	SSAScope::builder().CreateCall(SSAScope::module()->getFunction("calc_window_positions"), calc_window_positions_args);

	llvm::Value *calc_polygon_face_direction_args[] = { num_vertices.v, window_positions.v };
	SSABool front_facing_ccw = SSABool::from_llvm(SSAScope::builder().CreateCall(SSAScope::module()->getFunction("calc_polygon_face_direction"), calc_polygon_face_direction_args));

	SSAIfBlock cull_if;
	cull_if.if_block(front_facing_ccw);
	{
		SSAViewport viewport(viewport_x, viewport_y, viewport_width, viewport_height);

		SSAStack<SSAInt> y_start;
		SSAStack<SSAInt> y_end;

		llvm::Value *calc_polygon_y_range_args[] = { viewport_y.v, viewport_height.v, num_vertices.v, window_positions.v, y_start.v, y_end.v };
		SSAScope::builder().CreateCall(SSAScope::module()->getFunction("calc_polygon_y_range"), calc_polygon_y_range_args);

		y_start.store((y_start.load() + num_cores - core - 1) / num_cores * num_cores + core); // find_first_line_for_core

		SSAStack<SSAInt> stack_left_index;
		SSAStack<SSAInt> stack_right_index;
		SSAStack<SSAInt> stack_int_y;
		stack_left_index.store(0);
		stack_right_index.store(1);
		stack_int_y.store(y_start.load());
		SSAForBlock scanlines_loop;
		scanlines_loop.loop_block(stack_int_y.load() < y_end.load());
		{
			SSAInt int_y = stack_int_y.load();
			SSAFloat float_y = SSAFloat(int_y) + 0.5f;

			llvm::Value *update_polygon_edge_args0[] = { float_y.v, num_vertices.v, window_positions.v, stack_left_index.v, stack_right_index.v };
			llvm::Value *update_polygon_edge_args1[] = { float_y.v, num_vertices.v, window_positions.v, stack_right_index.v, stack_left_index.v };
			SSAScope::builder().CreateCall(SSAScope::module()->getFunction("update_polygon_edge"), update_polygon_edge_args0);
			SSAScope::builder().CreateCall(SSAScope::module()->getFunction("update_polygon_edge"), update_polygon_edge_args1);

			SSAInt left_index = stack_left_index.load();
			SSAInt right_index = stack_right_index.load();
			SSAInt next_left_index = (left_index + 1) % num_vertices;
			SSAInt next_right_index = (right_index + 1) % num_vertices;

			SSABarycentricWeight left_weight(viewport, fragment_ins[0][left_index].load(), fragment_ins[0][next_left_index].load());
			SSABarycentricWeight right_weight(viewport, fragment_ins[0][right_index].load(), fragment_ins[0][next_right_index].load());

			SSAFloat a = left_weight.from_window_y(int_y);
			SSAFloat b = right_weight.from_window_y(int_y);

			SSAVec4f left_clip_pos = left_weight.v1 * a + left_weight.v2 * (1.0f - a);
			SSAVec4f right_clip_pos = right_weight.v1 * b + right_weight.v2 * (1.0f - b);

			for (size_t i = 0; i + 1 < fragment_ins.size(); i++)
			{
				left_line_varyings[i].store(fragment_ins[i + 1][left_index].load() * a + fragment_ins[i + 1][next_left_index].load() * (1.0f - a));
				right_line_varyings[i].store(fragment_ins[i + 1][right_index].load() * b + fragment_ins[i + 1][next_right_index].load() * (1.0f - b));
			}

			llvm::Value *render_scanline_args[] = { output_width.v, output_height.v, output_data.v, viewport_x.v, viewport_y.v, viewport_width.v, viewport_height.v, int_y.v, left_clip_pos.v, right_clip_pos.v, left_line_varyings.v, right_line_varyings.v, input_width.v, input_height.v, input_data.v };
			SSAScope::builder().CreateCall(SSAScope::module()->getFunction("render_scanline"), render_scanline_args);

			stack_int_y.store(stack_int_y.load() + num_cores);
		}
		scanlines_loop.end_block();
	}
	cull_if.end_block();
}

void GlslFixedFunction::codegen_render_scanline(int num_varyings)
{
	llvm::IRBuilder<> builder(program.context());
	SSAScope ssa_scope(&program.context(), program.module(), &builder);

	SSAFunction function("render_scanline");
	function.add_parameter(SSAInt::llvm_type()); // output_width
	function.add_parameter(SSAInt::llvm_type()); // output_height
	function.add_parameter(SSAUBytePtr::llvm_type()); // output_data
	function.add_parameter(SSAInt::llvm_type()); // viewport_x
	function.add_parameter(SSAInt::llvm_type()); // viewport_y
	function.add_parameter(SSAInt::llvm_type()); // viewport_width
	function.add_parameter(SSAInt::llvm_type()); // viewport_height
	function.add_parameter(SSAInt::llvm_type()); // y
	function.add_parameter(SSAVec4f::llvm_type()); // left_clip_pos
	function.add_parameter(SSAVec4f::llvm_type()); // right_clip_pos
	function.add_parameter(SSAVec4fPtr::llvm_type()); // left_line_varyings
	function.add_parameter(SSAVec4fPtr::llvm_type()); // right_line_varyings
	function.add_parameter(SSAInt::llvm_type()); // input_width
	function.add_parameter(SSAInt::llvm_type()); // input_height
	function.add_parameter(SSAUBytePtr::llvm_type()); // input_data
	function.create_private();
	SSAInt output_width = function.parameter(0);
	SSAInt output_height = function.parameter(1);
	SSAUBytePtr output_data = function.parameter(2);
	SSAInt viewport_x = function.parameter(3);
	SSAInt viewport_y = function.parameter(4);
	SSAInt viewport_width = function.parameter(5);
	SSAInt viewport_height = function.parameter(6);
	SSAInt y = function.parameter(7);
	SSAVec4f left_clip_pos = function.parameter(8);
	SSAVec4f right_clip_pos = function.parameter(9);
	SSAVec4fPtr left_line_varyings = function.parameter(10);
	SSAVec4fPtr right_line_varyings = function.parameter(11);
	SSAInt input_width = function.parameter(12);
	SSAInt input_height = function.parameter(13);
	SSAUBytePtr input_data = function.parameter(14);

	SSAViewport viewport(viewport_x, viewport_y, viewport_width, viewport_height);

	SSAScopeHint hint;

	SSAStack<SSAInt> stack_x;
	SSAStack<SSAVec4f> stack_xnormalized;

	////////////////////////////////
	// Prepare to render scanline:

	hint.set("prepare");
	OuterData outer_data;

	SSAVec4f left_window_pos = viewport.clip_to_window(left_clip_pos);
	SSAVec4f right_window_pos = viewport.clip_to_window(right_clip_pos);

	SSAFloat x0 = left_window_pos[0];
	SSAFloat x1 = right_window_pos[0];
	SSAInt start(ssa_min(x0, x1));
	SSAInt end(ssa_max(x1, x0) + 0.5f);

	start = ssa_max(start, viewport.x);
	end = ssa_min(end, viewport.right);

	SSABarycentricWeight weight_scanline(viewport, left_clip_pos, right_clip_pos);

	outer_data.start = start;
	outer_data.end = end;
	outer_data.input_width = input_width;
	outer_data.input_height = input_height;
	outer_data.output_width = output_width;
	outer_data.output_height = output_height;
	outer_data.input_pixels = input_data;
	outer_data.output_pixels_line = output_data[output_width * y * 4];

	outer_data.viewport_x = SSAFloat(viewport.x);
	outer_data.viewport_rcp_half_width = viewport.rcp_half_width;
	outer_data.dx = weight_scanline.v2[0] - weight_scanline.v1[0];
	outer_data.dw = weight_scanline.v2[3] - weight_scanline.v1[3];
	outer_data.v1w = weight_scanline.v1[3];
	outer_data.v1x = weight_scanline.v1[0];
	outer_data.sse_left_varying_in = left_line_varyings;
	outer_data.sse_right_varying_in = right_line_varyings;
	outer_data.num_varyings = num_varyings;

	outer_data.sampler = SSAScope::alloca(get_sampler_struct(SSAScope::context()));
	std::vector<llvm::Value *> index_list;
	index_list.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)0)));
	index_list.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)0)));
	llvm::Value *sampler_width_ptr = SSAScope::builder().CreateGEP(outer_data.sampler, index_list);
	index_list[1] = llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)1));
	llvm::Value *sampler_height_ptr = SSAScope::builder().CreateGEP(outer_data.sampler, index_list);
	index_list[1] = llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)2));
	llvm::Value *sampler_data_ptr = SSAScope::builder().CreateGEP(outer_data.sampler, index_list);
	SSAScope::builder().CreateStore(outer_data.input_width.v, sampler_width_ptr, false);
	SSAScope::builder().CreateStore(outer_data.input_height.v, sampler_height_ptr, false);
	SSAScope::builder().CreateStore(outer_data.input_pixels.v, sampler_data_ptr, false);


	SSAVec4i xposinit = SSAVec4i(outer_data.start) + SSAVec4i(0, 1, 2, 3);
	stack_x.store(outer_data.start);
	stack_xnormalized.store((SSAVec4f(xposinit) + 0.5f - outer_data.viewport_x) * outer_data.viewport_rcp_half_width - 1.0f);

	/////////////////////////////////////////////////////////////////////////
	// First pixels:

	hint.set("firstpixels");
	SSAIfBlock if_block;
	if_block.if_block(outer_data.end - outer_data.start > 3);
	process_first_pixels(outer_data, stack_x, stack_xnormalized);
	if_block.end_block();

	/////////////////////////////////////////////////////////////////////////
	// Start: for (SSAInt x = start; x < end; x += 4)

	hint.set("loopstart");

	SSAForBlock for_block;
	SSAInt x = stack_x.load();
	for_block.loop_block(x + 3 < outer_data.end);

	/////////////////////////////////////////////////////////////////////////
	// Loop body
	{
		SSAVec4f xnormalized = stack_xnormalized.load();

		hint.set("blendload");
		SSAVec4i desti[4];
		SSAVec16ub dest_block = outer_data.output_pixels_line[x << 2].load_vec16ub();
		SSAVec4i::extend(dest_block, desti[0], desti[1], desti[2], desti[3]);

		SSAVec4f frag_colors[4];
		inner_block(outer_data, xnormalized, frag_colors);
		blend(frag_colors, dest_block);

		hint.set("blendstore");
		outer_data.output_pixels_line[x << 2].store_vec16ub(dest_block);
		hint.clear();

		xnormalized = xnormalized + 4.0f * outer_data.viewport_rcp_half_width;
		stack_xnormalized.store(xnormalized);
	}
	/////////////////////////////////////////////////////////////////////////
	// End: for (SSAInt x = start; x < end; x += 4)

	hint.set("loopend");
	x = x + 4;
	stack_x.store(x);
	for_block.end_block();

	/////////////////////////////////////////////////////////////////////////
	// Last pixels:

	hint.set("lastpixels");
	process_last_pixels(outer_data, stack_x, stack_xnormalized);

	builder.CreateRetVoid();
	llvm::verifyFunction(*function.func);
}

void GlslFixedFunction::process_first_pixels(OuterData &outer_data, SSAStack<SSAInt> &stack_x, SSAStack<SSAVec4f> &stack_xnormalized)
{
	SSAInt x = stack_x.load();
	SSAVec4f xnormalized = stack_xnormalized.load();
	SSAInt offset = x << 2;

	// Find how many pixels we have left until we 16 byte align:
	llvm::Value *output_line_align = SSAScope::builder().CreatePtrToInt(outer_data.output_pixels_line.v, llvm::Type::getInt32Ty(SSAScope::context()));
	output_line_align = SSAScope::builder().CreateAdd(output_line_align, offset.v);
	SSAInt left = 4 - (SSAInt::from_llvm(SSAScope::builder().CreateURem(output_line_align, SSAInt(16).v)) >> 2);

	SSAIfBlock if_block0;
	if_block0.if_block(left == 3);
	{
		SSAVec4i dest[4] =
		{
			outer_data.output_pixels_line[offset].load_vec4ub(),
			outer_data.output_pixels_line[offset + 4].load_vec4ub(),
			outer_data.output_pixels_line[offset + 8].load_vec4ub(),
			SSAVec4i(0)
		};

		// To do: do this in a less braindead way
		SSAVec16ub dest_block(SSAVec8s(dest[0], dest[1]), SSAVec8s(dest[2], dest[3]));
		SSAVec4f frag_colors[4];
		inner_block(outer_data, xnormalized, frag_colors);
		blend(frag_colors, dest_block);
		SSAVec4i::extend(dest_block, dest[0], dest[1], dest[2], dest[3]);

		outer_data.output_pixels_line[offset].store_vec4ub(dest[0]);
		outer_data.output_pixels_line[offset + 4].store_vec4ub(dest[1]);
		outer_data.output_pixels_line[offset + 8].store_vec4ub(dest[2]);

		stack_x.store(x + 3);
		stack_xnormalized.store(xnormalized + 3.0f * outer_data.viewport_rcp_half_width);
	}
	if_block0.else_block();
	{
		SSAIfBlock if_block1;
		if_block1.if_block(left == 2);
		{
			SSAVec4i dest[4] =
			{
				outer_data.output_pixels_line[offset].load_vec4ub(),
				outer_data.output_pixels_line[offset + 4].load_vec4ub(),
				SSAVec4i(0),
				SSAVec4i(0)
			};

			// To do: do this in a less braindead way
			SSAVec16ub dest_block(SSAVec8s(dest[0], dest[1]), SSAVec8s(dest[2], dest[3]));
			SSAVec4f frag_colors[4];
			inner_block(outer_data, xnormalized, frag_colors);
			blend(frag_colors, dest_block);
			SSAVec4i::extend(dest_block, dest[0], dest[1], dest[2], dest[3]);

			outer_data.output_pixels_line[offset].store_vec4ub(dest[0]);
			outer_data.output_pixels_line[offset + 4].store_vec4ub(dest[1]);

			stack_x.store(x + 2);
			stack_xnormalized.store(xnormalized + 2.0f * outer_data.viewport_rcp_half_width);
		}
		if_block1.else_block();
		{
			SSAIfBlock if_block2;
			if_block2.if_block(left == 1);
			{
				SSAVec4i dest[4] =
				{
					outer_data.output_pixels_line[offset].load_vec4ub(),
					SSAVec4i(0),
					SSAVec4i(0),
					SSAVec4i(0)
				};

				// To do: do this in a less braindead way
				SSAVec16ub dest_block(SSAVec8s(dest[0], dest[1]), SSAVec8s(dest[2], dest[3]));
				SSAVec4f frag_colors[4];
				inner_block(outer_data, xnormalized, frag_colors);
				blend(frag_colors, dest_block);
				SSAVec4i::extend(dest_block, dest[0], dest[1], dest[2], dest[3]);

				outer_data.output_pixels_line[offset].store_vec4ub(dest[0]);

				stack_x.store(x + 1);
				stack_xnormalized.store(xnormalized + outer_data.viewport_rcp_half_width);
			}
			if_block2.end_block();
		}
		if_block1.end_block();
	}
	if_block0.end_block();
}

void GlslFixedFunction::process_last_pixels(OuterData &outer_data, SSAStack<SSAInt> &stack_x, SSAStack<SSAVec4f> &stack_xnormalized)
{
	SSAInt x = stack_x.load();
	SSAVec4f xnormalized = stack_xnormalized.load();

	SSAInt left = outer_data.end - x;
	SSAInt offset = x << 2;
	SSAIfBlock if_block0;
	SSAIfBlock if_block1;
	SSAIfBlock if_block2;
	if_block0.if_block(left == 3);
	{
		SSAVec4i dest[4] =
		{
			outer_data.output_pixels_line[offset].load_vec4ub(),
			outer_data.output_pixels_line[offset + 4].load_vec4ub(),
			outer_data.output_pixels_line[offset + 8].load_vec4ub(),
			SSAVec4i(0)
		};

		// To do: do this in a less braindead way
		SSAVec16ub dest_block(SSAVec8s(dest[0], dest[1]), SSAVec8s(dest[2], dest[3]));
		SSAVec4f frag_colors[4];
		inner_block(outer_data, xnormalized, frag_colors);
		blend(frag_colors, dest_block);
		SSAVec4i::extend(dest_block, dest[0], dest[1], dest[2], dest[3]);

		outer_data.output_pixels_line[offset].store_vec4ub(dest[0]);
		outer_data.output_pixels_line[offset + 4].store_vec4ub(dest[1]);
		outer_data.output_pixels_line[offset + 8].store_vec4ub(dest[2]);
	}
	if_block0.else_block();
	if_block1.if_block(left == 2);
	{
		SSAVec4i dest[4] =
		{
			outer_data.output_pixels_line[offset].load_vec4ub(),
			outer_data.output_pixels_line[offset + 4].load_vec4ub(),
			SSAVec4i(0),
			SSAVec4i(0)
		};

		// To do: do this in a less braindead way
		SSAVec16ub dest_block(SSAVec8s(dest[0], dest[1]), SSAVec8s(dest[2], dest[3]));
		SSAVec4f frag_colors[4];
		inner_block(outer_data, xnormalized, frag_colors);
		blend(frag_colors, dest_block);
		SSAVec4i::extend(dest_block, dest[0], dest[1], dest[2], dest[3]);

		outer_data.output_pixels_line[offset].store_vec4ub(dest[0]);
		outer_data.output_pixels_line[offset + 4].store_vec4ub(dest[1]);
	}
	if_block1.else_block();
	if_block2.if_block(left == 1);
	{
		SSAVec4i dest[4] =
		{
			outer_data.output_pixels_line[offset].load_vec4ub(),
			SSAVec4i(0),
			SSAVec4i(0),
			SSAVec4i(0)
		};

		// To do: do this in a less braindead way
		SSAVec16ub dest_block(SSAVec8s(dest[0], dest[1]), SSAVec8s(dest[2], dest[3]));
		SSAVec4f frag_colors[4];
		inner_block(outer_data, xnormalized, frag_colors);
		blend(frag_colors, dest_block);
		SSAVec4i::extend(dest_block, dest[0], dest[1], dest[2], dest[3]);

		outer_data.output_pixels_line[offset].store_vec4ub(dest[0]);
	}
	if_block2.end_block();
	if_block1.end_block();
	if_block0.end_block();
}

void GlslFixedFunction::inner_block(OuterData &data, SSAVec4f xnormalized, SSAVec4f *frag_color)
{
	SSAScopeHint hint;
	hint.set("varying");
	SSAVec4f a = (xnormalized * data.v1w - data.v1x) * SSAVec4f::rcp(data.dx - xnormalized * data.dw);
	SSAVec4f one_minus_a = 1.0f - a;

	llvm::Value *globals_ptr[4];
	for (int i = 0; i < 4; i++)
	{
		globals_ptr[i] = SSAScope::alloca(fragment_codegen.get_global_struct_type());

		std::vector<llvm::Value *> index_list;
		index_list.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)0)));
		index_list.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)0)));
		llvm::Value *sampler_ptr = SSAScope::builder().CreateGEP(globals_ptr[i], index_list);
		SSAScope::builder().CreateStore(data.sampler, sampler_ptr, false);

		for (int j = 0; j < data.num_varyings; j++)
		{
			SSAVec4f field_value =
				data.sse_left_varying_in[j].load() * SSAVec4f::shuffle(one_minus_a, i, i, i, i) +
				data.sse_right_varying_in[j].load() * SSAVec4f::shuffle(a, i, i, i, i);
			index_list.clear();
			index_list.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)0)));
			index_list.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)j+1)));
			llvm::Value *field_ptr = SSAScope::builder().CreateGEP(globals_ptr[i], index_list);
			SSAScope::builder().CreateStore(field_value.v, field_ptr, false);
		}
	}

	hint.set("fragprogram");
	for (int i = 0; i < 4; i++)
	{
		SSAScope::builder().CreateCall(SSAScope::module()->getFunction((fragment_codegen.shader_prefix() + "main").c_str()), globals_ptr[i]);

		std::vector<llvm::Value *> index_list;
		index_list.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)0)));
		index_list.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)5)));
		llvm::Value *field_ptr = SSAScope::builder().CreateGEP(globals_ptr[i], index_list);
		frag_color[i] = SSAVec4f::from_llvm(SSAScope::builder().CreateLoad(field_ptr, false));
	}
}
/*
void GlslFixedFunction::blend(SSAVec4f frag_color[4], SSAVec16ub &dest)
{
	SSAVec4i desti[4];
	SSAVec4i::extend(dest, desti[0], desti[1], desti[2], desti[3]);

	// Pre-mulitiplied alpha blend:
	for (int pixel_index = 0; pixel_index < 4; pixel_index++)
	{
		SSAVec4f src = SSAVec4f::shuffle(frag_color[pixel_index], 2, 1, 0, 3);
		desti[pixel_index] = SSAVec4i(src * 255.0f);
		SSAVec4f dest = SSAVec4f(desti[pixel_index]) * (1.0f / 255.0f);
		SSAVec4f alpha = SSAVec4f::shuffle(dest, 3, 3, 3, 3);
		SSAVec4f resultf = src + dest * (1.0f - alpha);
		desti[pixel_index] = SSAVec4i(resultf * 255.0f);
	}

	dest = SSAVec16ub(SSAVec8s(desti[0], desti[1]), SSAVec8s(desti[2], desti[3]));
}
*/
void GlslFixedFunction::blend(SSAVec4f frag_color[4], SSAVec16ub &dest)
{
	for (int i = 0; i < 4; i++)
		frag_color[i] = SSAVec4f::shuffle(frag_color[i], 2, 1, 0, 3);

	// Pre-mulitiplied alpha blend:
	SSAVec8s dest0 = SSAVec8s::extendlo(dest);
	SSAVec8s dest1 = SSAVec8s::extendhi(dest);

	SSAVec8s src0(SSAVec4i(frag_color[0] * 255.0f), SSAVec4i(frag_color[1] * 255.0f));
	SSAVec8s src1(SSAVec4i(frag_color[2] * 255.0f), SSAVec4i(frag_color[3] * 255.0f));

	// Extract and duplicate alpha components:
	SSAVec8s alpha0 = SSAVec8s::shuffle(src0, 3, 3, 3, 3, 7, 7, 7, 7);
	SSAVec8s alpha1 = SSAVec8s::shuffle(src1, 3, 3, 3, 3, 7, 7, 7, 7);

	// Convert from 0-255 to 0-256 range:
	alpha0 = SSAVec8s::max_sse2(alpha0, 255);
	alpha1 = SSAVec8s::max_sse2(alpha1, 255);
	alpha0 = alpha0 + (alpha0 >> 7);
	alpha1 = alpha1 + (alpha1 >> 7);

	SSAVec8s result0 = src0 + ((dest0 * (256 - alpha0)) >> 8);
	SSAVec8s result1 = src1 + ((dest1 * (256 - alpha1)) >> 8);

	dest = SSAVec16ub(result0, result1);
}

#endif
