
#pragma once

#include "ssa_bool.h"
#include "r_compiler/llvm_include.h"

class SSAForBlock
{
public:
	SSAForBlock();
	void loop_block(SSABool true_condition);
	void end_block();

private:
	llvm::BasicBlock *if_basic_block;
	llvm::BasicBlock *loop_basic_block;
	llvm::BasicBlock *end_basic_block;
};
