
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

void SSAForBlock::loop_block(SSABool true_condition)
{
	SSAScope::builder().CreateCondBr(true_condition.v, loop_basic_block, end_basic_block);
	SSAScope::builder().SetInsertPoint(loop_basic_block);
}

void SSAForBlock::end_block()
{
	SSAScope::builder().CreateBr(if_basic_block);
	SSAScope::builder().SetInsertPoint(end_basic_block);
}
