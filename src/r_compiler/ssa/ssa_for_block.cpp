
#include "r_compiler/llvm_include.h"
#include "ssa_for_block.h"
#include "ssa_scope.h"

SSAForBlock::SSAForBlock()
: if_basic_block(0), loop_basic_block(0), end_basic_block(0)
{
	if_basic_block = llvm::BasicBlock::Create(SSAScope::context(), "forbegin", SSAScope::builder().GetInsertBlock()->getParent());
	loop_basic_block = llvm::BasicBlock::Create(SSAScope::context(), "forloop", SSAScope::builder().GetInsertBlock()->getParent());
	end_basic_block = llvm::BasicBlock::Create(SSAScope::context(), "forend", SSAScope::builder().GetInsertBlock()->getParent());
	SSAScope::builder().CreateBr(if_basic_block);
	SSAScope::builder().SetInsertPoint(if_basic_block);
}

void SSAForBlock::loop_block(SSABool true_condition, int unroll_count)
{
	auto branch = SSAScope::builder().CreateCondBr(true_condition.v, loop_basic_block, end_basic_block);
#if LLVM_VERSION_MAJOR > 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 9)
	if (unroll_count > 0)
	{
		using namespace llvm;
		auto md_unroll_enable = MDNode::get(SSAScope::context(), {
			MDString::get(SSAScope::context(), "llvm.loop.unroll.enable")
		});
		auto md_unroll_count = MDNode::get(SSAScope::context(), {
			MDString::get(SSAScope::context(), "llvm.loop.unroll.count"),
			ConstantAsMetadata::get(ConstantInt::get(SSAScope::context(), APInt(32, unroll_count)))
		});
		auto md_loop = MDNode::getDistinct(SSAScope::context(), { md_unroll_enable, md_unroll_count });
		branch->setMetadata(LLVMContext::MD_loop, md_loop);
	}
#endif
	SSAScope::builder().SetInsertPoint(loop_basic_block);
}

void SSAForBlock::end_block()
{
	SSAScope::builder().CreateBr(if_basic_block);
	SSAScope::builder().SetInsertPoint(end_basic_block);
}
