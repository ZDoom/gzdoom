/*
**  SSA value
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

#include "precomp.h"
#include "ssa_value.h"
#include "ssa_int.h"
#include "ssa_scope.h"

SSAValue SSAValue::load(bool constantScopeDomain)
{
	auto loadInst = SSAScope::builder().CreateLoad(v, false);
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAValue::from_llvm(loadInst);
}

void SSAValue::store(llvm::Value *value)
{
	auto inst = SSAScope::builder().CreateStore(value, v, false);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

SSAIndexLookup SSAValue::operator[](int index)
{
	SSAIndexLookup result;
	result.v = v;
	result.indexes.push_back(SSAInt(index).v);
	return result;
}

SSAIndexLookup SSAValue::operator[](SSAInt index)
{
	SSAIndexLookup result;
	result.v = v;
	result.indexes.push_back(index.v);
	return result;
}

/////////////////////////////////////////////////////////////////////////////

SSAIndexLookup::operator SSAValue()
{
	return SSAValue::from_llvm(SSAScope::builder().CreateGEP(v, indexes));
}

SSAIndexLookup SSAIndexLookup::operator[](int index)
{
	SSAIndexLookup result;
	result.v = v;
	result.indexes = indexes;
	result.indexes.push_back(SSAInt(index).v);
	return result;
}

SSAIndexLookup SSAIndexLookup::operator[](SSAInt index)
{
	SSAIndexLookup result;
	result.v = v;
	result.indexes = indexes;
	result.indexes.push_back(index.v);
	return result;
}
