
#pragma once

#include "ssa_scope.h"

class SSAIfBlock;

template <typename SSAVariable>
class SSAPhi
{
public:
	void add_incoming(SSAVariable var)
	{
		incoming.push_back(Incoming(var.v, SSAScope::builder().GetInsertBlock()));
	}

	SSAVariable create()
	{
		llvm::PHINode *phi_node = SSAScope::builder().CreatePHI(SSAVariable::llvm_type(), (unsigned int)incoming.size(), SSAScope::hint());
		for (size_t i = 0; i < incoming.size(); i++)
			phi_node->addIncoming(incoming[i].v, incoming[i].bb);
		return SSAVariable::from_llvm(phi_node);
	}

private:
	struct Incoming
	{
		Incoming(llvm::Value *v, llvm::BasicBlock *bb) : v(v), bb(bb) { }
		llvm::Value *v;
		llvm::BasicBlock *bb;
	};
	std::vector<Incoming> incoming;
};
