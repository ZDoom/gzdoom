/*
**  SSA vec4 int32 pointer
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
#include "ssa_vec4i_ptr.h"
#include "ssa_scope.h"

SSAVec4iPtr::SSAVec4iPtr()
: v(0)
{
}

SSAVec4iPtr::SSAVec4iPtr(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAVec4iPtr::llvm_type()
{
	return llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
}

SSAVec4iPtr SSAVec4iPtr::operator[](SSAInt index) const
{
	return SSAVec4iPtr::from_llvm(SSAScope::builder().CreateGEP(v, index.v, SSAScope::hint()));
}

SSAVec4i SSAVec4iPtr::load() const
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateLoad(v, false, SSAScope::hint()));
}

SSAVec4i SSAVec4iPtr::load_unaligned() const
{
	return SSAVec4i::from_llvm(SSAScope::builder().Insert(new llvm::LoadInst(v, SSAScope::hint(), false, 4)));
}

void SSAVec4iPtr::store(const SSAVec4i &new_value)
{
	SSAScope::builder().CreateStore(new_value.v, v, false);
}

void SSAVec4iPtr::store_unaligned(const SSAVec4i &new_value)
{
	SSAScope::builder().CreateAlignedStore(new_value.v, v, 4, false);
}
