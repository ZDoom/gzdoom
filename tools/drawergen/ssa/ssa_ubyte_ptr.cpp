/*
**  SSA uint8 pointer
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
#include "ssa_ubyte_ptr.h"
#include "ssa_scope.h"
#include "ssa_bool.h"

SSAUBytePtr::SSAUBytePtr()
: v(0)
{
}

SSAUBytePtr::SSAUBytePtr(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAUBytePtr::llvm_type()
{
	return llvm::Type::getInt8PtrTy(SSAScope::context());
}

SSAUBytePtr SSAUBytePtr::operator[](SSAInt index) const
{
	return SSAUBytePtr::from_llvm(SSAScope::builder().CreateGEP(v, index.v, SSAScope::hint()));
}

SSAUByte SSAUBytePtr::load(bool constantScopeDomain) const
{
	auto loadInst = SSAScope::builder().CreateLoad(v, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAUByte::from_llvm(loadInst);
}

SSAVec4i SSAUBytePtr::load_vec4ub(bool constantScopeDomain) const
{
	auto loadInst = SSAScope::builder().CreateLoad(SSAScope::builder().CreateBitCast(v, llvm::Type::getInt32PtrTy(SSAScope::context()), SSAScope::hint()), false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	SSAInt i32 = SSAInt::from_llvm(loadInst);
	return SSAVec4i::unpack(i32);
}

SSAVec16ub SSAUBytePtr::load_vec16ub(bool constantScopeDomain) const
{
	llvm::PointerType *m16xint8typeptr = llvm::VectorType::get(llvm::Type::getInt8Ty(SSAScope::context()), 16)->getPointerTo();
	auto loadInst = SSAScope::builder().CreateAlignedLoad(SSAScope::builder().CreateBitCast(v, m16xint8typeptr, SSAScope::hint()), 16, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAVec16ub::from_llvm(loadInst);
}

SSAVec16ub SSAUBytePtr::load_unaligned_vec16ub(bool constantScopeDomain) const
{
	llvm::PointerType *m16xint8typeptr = llvm::VectorType::get(llvm::Type::getInt8Ty(SSAScope::context()), 16)->getPointerTo();
	auto loadInst = SSAScope::builder().CreateAlignedLoad(SSAScope::builder().CreateBitCast(v, m16xint8typeptr, SSAScope::hint()), 1, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAVec16ub::from_llvm(loadInst);
}

void SSAUBytePtr::store(const SSAUByte &new_value)
{
	auto inst = SSAScope::builder().CreateStore(new_value.v, v, false);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

void SSAUBytePtr::store_vec4ub(const SSAVec4i &new_value)
{
	// Store using saturate:
	SSAVec8s v8s(new_value, new_value);
	SSAVec16ub v16ub(v8s, v8s);
	
	llvm::Type *m16xint8type = llvm::VectorType::get(llvm::Type::getInt8Ty(SSAScope::context()), 16);
	llvm::PointerType *m4xint8typeptr = llvm::VectorType::get(llvm::Type::getInt8Ty(SSAScope::context()), 4)->getPointerTo();
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 1)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 2)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 3)));
	llvm::Value *mask = llvm::ConstantVector::get(constants);
	llvm::Value *val_vector = SSAScope::builder().CreateShuffleVector(v16ub.v, llvm::UndefValue::get(m16xint8type), mask, SSAScope::hint());
	llvm::StoreInst *inst = SSAScope::builder().CreateStore(val_vector, SSAScope::builder().CreateBitCast(v, m4xint8typeptr, SSAScope::hint()), false);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

void SSAUBytePtr::store_masked_vec4ub(const SSAVec4i &new_value, SSABool mask[4])
{
	// Store using saturate:
	SSAVec8s v8s(new_value, new_value);
	SSAVec16ub v16ub(v8s, v8s);

	// Create mask vector
	std::vector<llvm::Constant*> maskconstants;
	maskconstants.resize(4, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(1, 0, false)));
	llvm::Value *maskValue = llvm::ConstantVector::get(maskconstants);
#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 9)
	for (int i = 0; i < 4; i++)
		maskValue = SSAScope::builder().CreateInsertElement(maskValue, mask[i].v, SSAInt(i).v, SSAScope::hint());
#else
	for (int i = 0; i < 4; i++)
		maskValue = SSAScope::builder().CreateInsertElement(maskValue, mask[i].v, (uint64_t)i, SSAScope::hint());
#endif

	llvm::Type *m16xint8type = llvm::VectorType::get(llvm::Type::getInt8Ty(SSAScope::context()), 16);
	llvm::PointerType *m4xint8typeptr = llvm::VectorType::get(llvm::Type::getInt8Ty(SSAScope::context()), 4)->getPointerTo();
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 1)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 2)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 3)));
	llvm::Value *shufflemask = llvm::ConstantVector::get(constants);
	llvm::Value *val_vector = SSAScope::builder().CreateShuffleVector(v16ub.v, llvm::UndefValue::get(m16xint8type), shufflemask, SSAScope::hint());
	llvm::CallInst *inst = SSAScope::builder().CreateMaskedStore(val_vector, SSAScope::builder().CreateBitCast(v, m4xint8typeptr, SSAScope::hint()), 1, maskValue);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

void SSAUBytePtr::store_vec16ub(const SSAVec16ub &new_value)
{
	llvm::PointerType *m16xint8typeptr = llvm::VectorType::get(llvm::Type::getInt8Ty(SSAScope::context()), 16)->getPointerTo();
	llvm::StoreInst *inst = SSAScope::builder().CreateAlignedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m16xint8typeptr, SSAScope::hint()), 16);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());

	// The following generates _mm_stream_si128, maybe!
	// llvm::MDNode *node = llvm::MDNode::get(SSAScope::context(), SSAScope::builder().getInt32(1));
	// inst->setMetadata(SSAScope::module()->getMDKindID("nontemporal"), node);
}

void SSAUBytePtr::store_unaligned_vec16ub(const SSAVec16ub &new_value)
{
	llvm::PointerType *m16xint8typeptr = llvm::VectorType::get(llvm::Type::getInt8Ty(SSAScope::context()), 16)->getPointerTo();
	llvm::StoreInst *inst = SSAScope::builder().CreateAlignedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m16xint8typeptr, SSAScope::hint()), 4);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

void SSAUBytePtr::store_masked_vec16ub(const SSAVec16ub &new_value, SSABool mask[4])
{
	std::vector<llvm::Constant*> constants;
	constants.resize(16, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(1, 0, false)));
	llvm::Value *maskValue = llvm::ConstantVector::get(constants);
#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 9)
	for (int i = 0; i < 16; i++)
		maskValue = SSAScope::builder().CreateInsertElement(maskValue, mask[i / 4].v, SSAInt(i).v, SSAScope::hint());
#else
	for (int i = 0; i < 16; i++)
		maskValue = SSAScope::builder().CreateInsertElement(maskValue, mask[i / 4].v, (uint64_t)i, SSAScope::hint());
#endif

	llvm::PointerType *m16xint8typeptr = llvm::VectorType::get(llvm::Type::getInt8Ty(SSAScope::context()), 16)->getPointerTo();
	llvm::CallInst *inst = SSAScope::builder().CreateMaskedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m16xint8typeptr, SSAScope::hint()), 1, maskValue);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}
