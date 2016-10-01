
#pragma once

#include "ssa_bool.h"
#include "ssa_phi.h"

class SSAIfBlock
{
public:
	SSAIfBlock();
	void if_block(SSABool true_condition);
	void else_block();
	void end_block();

private:
	llvm::BasicBlock *if_basic_block;
	llvm::BasicBlock *else_basic_block;
	llvm::BasicBlock *end_basic_block;
};

template<typename T>
T ssa_min(T a, T b)
{
	SSAPhi<T> phi;
	SSAIfBlock if_block;
	if_block.if_block(a <= b);
	phi.add_incoming(a);
	if_block.else_block();
	phi.add_incoming(b);
	if_block.end_block();
	return phi.create();
}

template<typename T>
T ssa_max(T a, T b)
{
	SSAPhi<T> phi;
	SSAIfBlock if_block;
	if_block.if_block(a >= b);
	phi.add_incoming(a);
	if_block.else_block();
	phi.add_incoming(b);
	if_block.end_block();
	return phi.create();
}
