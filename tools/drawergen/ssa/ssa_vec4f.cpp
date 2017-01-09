/*
**  SSA vec4 float
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
#include "ssa_vec4f.h"
#include "ssa_vec4i.h"
#include "ssa_float.h"
#include "ssa_int.h"
#include "ssa_scope.h"

SSAVec4f::SSAVec4f()
: v(0)
{
}

SSAVec4f::SSAVec4f(float constant)
: v(0)
{
	std::vector<llvm::Constant*> constants;
	constants.resize(4, llvm::ConstantFP::get(SSAScope::context(), llvm::APFloat(constant)));
	v = llvm::ConstantVector::get(constants);
}

SSAVec4f::SSAVec4f(float constant0, float constant1, float constant2, float constant3)
: v(0)
{
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantFP::get(SSAScope::context(), llvm::APFloat(constant0)));
	constants.push_back(llvm::ConstantFP::get(SSAScope::context(), llvm::APFloat(constant1)));
	constants.push_back(llvm::ConstantFP::get(SSAScope::context(), llvm::APFloat(constant2)));
	constants.push_back(llvm::ConstantFP::get(SSAScope::context(), llvm::APFloat(constant3)));
	v = llvm::ConstantVector::get(constants);
}

SSAVec4f::SSAVec4f(SSAFloat f)
: v(0)
{
	llvm::Type *m1xfloattype = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 1);
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	llvm::Value *mask = llvm::ConstantVector::get(constants);
	v = SSAScope::builder().CreateShuffleVector(SSAScope::builder().CreateBitCast(f.v, m1xfloattype, SSAScope::hint()), llvm::UndefValue::get(m1xfloattype), mask, SSAScope::hint());
}

SSAVec4f::SSAVec4f(SSAFloat f0, SSAFloat f1, SSAFloat f2, SSAFloat f3)
: v(0)
{
	v = SSAScope::builder().CreateInsertElement(llvm::UndefValue::get(llvm_type()), f0.v, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)0)));
	v = SSAScope::builder().CreateInsertElement(v, f1.v, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)1)));
	v = SSAScope::builder().CreateInsertElement(v, f2.v, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)2)));
	v = SSAScope::builder().CreateInsertElement(v, f3.v, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)3)));
}

SSAVec4f::SSAVec4f(llvm::Value *v)
: v(v)
{
}

SSAVec4f::SSAVec4f(SSAVec4i i32)
: v(0)
{
#ifdef ARM_TARGET
	v = SSAScope::builder().CreateSIToFP(i32.v, llvm_type(), SSAScope::hint());
#else
	//llvm::VectorType *m128type = llvm::VectorType::get(llvm::Type::getFloatTy(*context), 4);
	//return builder->CreateSIToFP(i32.v, m128type);
	v = SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::x86_sse2_cvtdq2ps), i32.v, SSAScope::hint());
#endif
}

llvm::Type *SSAVec4f::llvm_type()
{
	return llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4);
}

SSAFloat SSAVec4f::operator[](SSAInt index) const
{
	return SSAFloat::from_llvm(SSAScope::builder().CreateExtractElement(v, index.v, SSAScope::hint()));
}

SSAFloat SSAVec4f::operator[](int index) const
{
	return (*this)[SSAInt(index)];
}

SSAVec4f SSAVec4f::insert_element(SSAVec4f vec4f, SSAFloat value, int index)
{
	return from_llvm(SSAScope::builder().CreateInsertElement(vec4f.v, value.v, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)index))));
}

SSAVec4f SSAVec4f::bitcast(SSAVec4i i32)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateBitCast(i32.v, llvm_type(), SSAScope::hint()));
}

void SSAVec4f::transpose(SSAVec4f &row0, SSAVec4f &row1, SSAVec4f &row2, SSAVec4f &row3)
{
	SSAVec4f tmp0 = shuffle(row0, row1, 0x44);//_MM_SHUFFLE(1,0,1,0));
	SSAVec4f tmp2 = shuffle(row0, row1, 0xEE);//_MM_SHUFFLE(3,2,3,2));
	SSAVec4f tmp1 = shuffle(row2, row3, 0x44);//_MM_SHUFFLE(1,0,1,0));
	SSAVec4f tmp3 = shuffle(row2, row3, 0xEE);//_MM_SHUFFLE(3,2,3,2));
	row0 = shuffle(tmp0, tmp1, 0x88);//_MM_SHUFFLE(2,0,2,0));
	row1 = shuffle(tmp0, tmp1, 0xDD);//_MM_SHUFFLE(3,1,3,1));
	row2 = shuffle(tmp2, tmp3, 0x88);//_MM_SHUFFLE(2,0,2,0));
	row3 = shuffle(tmp2, tmp3, 0xDD);//_MM_SHUFFLE(3,1,3,1));
}

SSAVec4f SSAVec4f::shuffle(const SSAVec4f &f0, int index0, int index1, int index2, int index3)
{
	return shuffle(f0, from_llvm(llvm::UndefValue::get(llvm_type())), index0, index1, index2, index3);
}

SSAVec4f SSAVec4f::shuffle(const SSAVec4f &f0, const SSAVec4f &f1, int index0, int index1, int index2, int index3)
{
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index1)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index2)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index3)));
	llvm::Value *mask = llvm::ConstantVector::get(constants);
	return SSAVec4f::from_llvm(SSAScope::builder().CreateShuffleVector(f0.v, f1.v, mask, SSAScope::hint()));
}

SSAVec4f SSAVec4f::shuffle(const SSAVec4f &f0, const SSAVec4f &f1, int mask)
{
	return shuffle(f0, f1, mask & 3, (mask >> 2) & 3, ((mask >> 4) & 3) + 4, ((mask >> 6) & 3) + 4);
}

SSAVec4f operator+(const SSAVec4f &a, const SSAVec4f &b)
{
	return SSAVec4f::from_llvm(SSAScope::builder().CreateFAdd(a.v, b.v, SSAScope::hint()));
}

SSAVec4f operator-(const SSAVec4f &a, const SSAVec4f &b)
{
	return SSAVec4f::from_llvm(SSAScope::builder().CreateFSub(a.v, b.v, SSAScope::hint()));
}

SSAVec4f operator*(const SSAVec4f &a, const SSAVec4f &b)
{
	return SSAVec4f::from_llvm(SSAScope::builder().CreateFMul(a.v, b.v, SSAScope::hint()));
}

SSAVec4f operator/(const SSAVec4f &a, const SSAVec4f &b)
{
	return SSAVec4f::from_llvm(SSAScope::builder().CreateFDiv(a.v, b.v, SSAScope::hint()));
}

SSAVec4f operator+(float a, const SSAVec4f &b)
{
	return SSAVec4f(a) + b;
}

SSAVec4f operator-(float a, const SSAVec4f &b)
{
	return SSAVec4f(a) - b;
}

SSAVec4f operator*(float a, const SSAVec4f &b)
{
	return SSAVec4f(a) * b;
}

SSAVec4f operator/(float a, const SSAVec4f &b)
{
	return SSAVec4f(a) / b;
}

SSAVec4f operator+(const SSAVec4f &a, float b)
{
	return a + SSAVec4f(b);
}

SSAVec4f operator-(const SSAVec4f &a, float b)
{
	return a - SSAVec4f(b);
}

SSAVec4f operator*(const SSAVec4f &a, float b)
{
	return a * SSAVec4f(b);
}

SSAVec4f operator/(const SSAVec4f &a, float b)
{
	return a / SSAVec4f(b);
}
