
#include "r_compiler/llvm_include.h"
#include "ssa_if_block.h"
#include "ssa_scope.h"

SSAIfBlock::SSAIfBlock()
: if_basic_block(0), else_basic_block(0), end_basic_block(0)
{
}

void SSAIfBlock::if_block(SSABool true_condition)
{
	if_basic_block = llvm::BasicBlock::Create(SSAScope::context(), "if", SSAScope::builder().GetInsertBlock()->getParent());
	else_basic_block = llvm::BasicBlock::Create(SSAScope::context(), "else", SSAScope::builder().GetInsertBlock()->getParent());
	end_basic_block = else_basic_block;
	SSAScope::builder().CreateCondBr(true_condition.v, if_basic_block, else_basic_block);
	SSAScope::builder().SetInsertPoint(if_basic_block);
}

void SSAIfBlock::else_block()
{
	end_basic_block = llvm::BasicBlock::Create(SSAScope::context(), "end", SSAScope::builder().GetInsertBlock()->getParent());
	SSAScope::builder().CreateBr(end_basic_block);
	SSAScope::builder().SetInsertPoint(else_basic_block);
}

void SSAIfBlock::end_block()
{
	SSAScope::builder().CreateBr(end_basic_block);
	SSAScope::builder().SetInsertPoint(end_basic_block);
}
