/*
**  LLVM if statement branching
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

void SSAIfBlock::end_retvoid()
{
	SSAScope::builder().CreateRetVoid();
	SSAScope::builder().SetInsertPoint(end_basic_block);
}
