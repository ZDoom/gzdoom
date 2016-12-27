/*
**  LLVM stack variable
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

template<typename SSAVariable>
class SSAStack
{
public:
	SSAStack()
	: v(0)
	{
		v = SSAScope::alloc_stack(SSAVariable::llvm_type());
	}

	SSAVariable load() const
	{
		return SSAVariable::from_llvm(SSAScope::builder().CreateLoad(v, SSAScope::hint()));
	}

	void store(const SSAVariable &new_value)
	{
		SSAScope::builder().CreateStore(new_value.v, v);
	}

	llvm::Value *v;
};
