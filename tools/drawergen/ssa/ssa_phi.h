/*
**  SSA phi node
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
