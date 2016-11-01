/*
**  SSA vec4 int32
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
#include "ssa_vec4i.h"
#include "ssa_vec4f.h"
#include "ssa_vec8s.h"
#include "ssa_vec16ub.h"
#include "ssa_int.h"
#include "ssa_scope.h"

SSAVec4i::SSAVec4i()
: v(0)
{
}

SSAVec4i::SSAVec4i(int constant)
: v(0)
{
	std::vector<llvm::Constant*> constants;
	constants.resize(4, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, constant, true)));
	v = llvm::ConstantVector::get(constants);
}

SSAVec4i::SSAVec4i(int constant0, int constant1, int constant2, int constant3)
: v(0)
{
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, constant0, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, constant1, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, constant2, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, constant3, true)));
	v = llvm::ConstantVector::get(constants);
}

SSAVec4i::SSAVec4i(llvm::Value *v)
: v(v)
{
}

SSAVec4i::SSAVec4i(SSAInt i)
: v(0)
{
	llvm::Type *m1xi32type = llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 1);
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0)));
	llvm::Value *mask = llvm::ConstantVector::get(constants);
	v = SSAScope::builder().CreateShuffleVector(SSAScope::builder().CreateBitCast(i.v, m1xi32type, SSAScope::hint()), llvm::UndefValue::get(m1xi32type), mask, SSAScope::hint());
}

SSAVec4i::SSAVec4i(SSAInt i0, SSAInt i1, SSAInt i2, SSAInt i3)
: v(0)
{
	std::vector<llvm::Constant*> constants;
	constants.resize(4, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, 0, true)));
	v = llvm::ConstantVector::get(constants);
#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 9)
	v = SSAScope::builder().CreateInsertElement(v, i0.v, SSAInt(0).v, SSAScope::hint());
	v = SSAScope::builder().CreateInsertElement(v, i1.v, SSAInt(1).v, SSAScope::hint());
	v = SSAScope::builder().CreateInsertElement(v, i2.v, SSAInt(2).v, SSAScope::hint());
	v = SSAScope::builder().CreateInsertElement(v, i3.v, SSAInt(3).v, SSAScope::hint());
#else
	v = SSAScope::builder().CreateInsertElement(v, i0.v, (uint64_t)0, SSAScope::hint());
	v = SSAScope::builder().CreateInsertElement(v, i1.v, (uint64_t)1, SSAScope::hint());
	v = SSAScope::builder().CreateInsertElement(v, i2.v, (uint64_t)2, SSAScope::hint());
	v = SSAScope::builder().CreateInsertElement(v, i3.v, (uint64_t)3, SSAScope::hint());
#endif
}

SSAVec4i::SSAVec4i(SSAVec4f f32)
: v(0)
{
	v = SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::x86_sse2_cvttps2dq), f32.v, SSAScope::hint());
}

SSAInt SSAVec4i::operator[](SSAInt index) const
{
	return SSAInt::from_llvm(SSAScope::builder().CreateExtractElement(v, index.v, SSAScope::hint()));
}

SSAInt SSAVec4i::operator[](int index) const
{
	return (*this)[SSAInt(index)];
}

SSAVec4i SSAVec4i::insert(SSAInt index, SSAInt value)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateInsertElement(v, value.v, index.v, SSAScope::hint()));
}

SSAVec4i SSAVec4i::insert(int index, SSAInt value)
{
#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 9)
	return SSAVec4i::from_llvm(SSAScope::builder().CreateInsertElement(v, value.v, SSAInt(index).v, SSAScope::hint()));
#else
	return SSAVec4i::from_llvm(SSAScope::builder().CreateInsertElement(v, value.v, index, SSAScope::hint()));
#endif
}

SSAVec4i SSAVec4i::insert(int index, int value)
{
	return insert(index, SSAInt(value));
}

llvm::Type *SSAVec4i::llvm_type()
{
	return llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4);
}

SSAVec4i SSAVec4i::unpack(SSAInt i32)
{
	// _mm_cvtsi32_si128 as implemented by clang:
	llvm::Value *v = SSAScope::builder().CreateInsertElement(llvm::UndefValue::get(SSAVec4i::llvm_type()), i32.v, SSAInt(0).v, SSAScope::hint());
	v = SSAScope::builder().CreateInsertElement(v, SSAInt(0).v, SSAInt(1).v, SSAScope::hint());
	v = SSAScope::builder().CreateInsertElement(v, SSAInt(0).v, SSAInt(2).v, SSAScope::hint());
	v = SSAScope::builder().CreateInsertElement(v, SSAInt(0).v, SSAInt(3).v, SSAScope::hint());
	SSAVec4i v4i = SSAVec4i::from_llvm(v);

	SSAVec8s low = SSAVec8s::bitcast(SSAVec16ub::shuffle(SSAVec16ub::bitcast(v4i), SSAVec16ub((unsigned char)0), 0, 16 + 0, 1, 16 + 1, 2, 16 + 2, 3, 16 + 3, 4, 16 + 4, 5, 16 + 5, 6, 16 + 6, 7, 16 + 7)); // _mm_unpacklo_epi8
	return SSAVec4i::extendlo(low); // _mm_unpacklo_epi16
}

SSAVec4i SSAVec4i::bitcast(SSAVec4f f32)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateBitCast(f32.v, llvm_type(), SSAScope::hint()));
}

SSAVec4i SSAVec4i::bitcast(SSAVec8s i16)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateBitCast(i16.v, llvm_type(), SSAScope::hint()));
}

SSAVec4i SSAVec4i::shuffle(const SSAVec4i &i0, int index0, int index1, int index2, int index3)
{
	return shuffle(i0, from_llvm(llvm::UndefValue::get(llvm_type())), index0, index1, index2, index3);
}

SSAVec4i SSAVec4i::shuffle(const SSAVec4i &i0, const SSAVec4i &i1, int index0, int index1, int index2, int index3)
{
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index1)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index2)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index3)));
	llvm::Value *mask = llvm::ConstantVector::get(constants);
	return SSAVec4i::from_llvm(SSAScope::builder().CreateShuffleVector(i0.v, i1.v, mask, SSAScope::hint()));
}

void SSAVec4i::extend(SSAVec16ub a, SSAVec4i &out0, SSAVec4i &out1, SSAVec4i &out2, SSAVec4i &out3)
{
	SSAVec8s low = SSAVec8s::extendlo(a);
	SSAVec8s high = SSAVec8s::extendhi(a);
	out0 = extendlo(low);
	out1 = extendhi(low);
	out2 = extendlo(high);
	out3 = extendhi(high);
}

SSAVec4i SSAVec4i::extendhi(SSAVec8s i16)
{
	return SSAVec4i::bitcast(SSAVec8s::shuffle(i16, SSAVec8s((short)0), 4, 8+4, 5, 8+5, 6, 8+6, 7, 8+7)); // _mm_unpackhi_epi16
}

SSAVec4i SSAVec4i::extendlo(SSAVec8s i16)
{
	return SSAVec4i::bitcast(SSAVec8s::shuffle(i16, SSAVec8s((short)0), 0, 8+0, 1, 8+1, 2, 8+2, 3, 8+3)); // _mm_unpacklo_epi16
}

SSAVec4i SSAVec4i::combinehi(SSAVec8s a, SSAVec8s b)
{
	return SSAVec4i::bitcast(SSAVec8s::shuffle(a, b, 4, 8+4, 5, 8+5, 6, 8+6, 7, 8+7)); // _mm_unpackhi_epi16
}

SSAVec4i SSAVec4i::combinelo(SSAVec8s a, SSAVec8s b)
{
	return SSAVec4i::bitcast(SSAVec8s::shuffle(a, b, 0, 8+0, 1, 8+1, 2, 8+2, 3, 8+3)); // _mm_unpacklo_epi16
}

SSAVec4i SSAVec4i::sqrt(SSAVec4i f)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::x86_sse2_sqrt_pd), f.v, SSAScope::hint()));
}

SSAVec4i operator+(const SSAVec4i &a, const SSAVec4i &b)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateAdd(a.v, b.v, SSAScope::hint()));
}

SSAVec4i operator-(const SSAVec4i &a, const SSAVec4i &b)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateSub(a.v, b.v, SSAScope::hint()));
}

SSAVec4i operator*(const SSAVec4i &a, const SSAVec4i &b)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateMul(a.v, b.v, SSAScope::hint()));
}

SSAVec4i operator/(const SSAVec4i &a, const SSAVec4i &b)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateSDiv(a.v, b.v, SSAScope::hint()));
}

SSAVec4i operator+(int a, const SSAVec4i &b)
{
	return SSAVec4i(a) + b;
}

SSAVec4i operator-(int a, const SSAVec4i &b)
{
	return SSAVec4i(a) - b;
}

SSAVec4i operator*(int a, const SSAVec4i &b)
{
	return SSAVec4i(a) * b;
}

SSAVec4i operator/(int a, const SSAVec4i &b)
{
	return SSAVec4i(a) / b;
}

SSAVec4i operator+(const SSAVec4i &a, int b)
{
	return a + SSAVec4i(b);
}

SSAVec4i operator-(const SSAVec4i &a, int b)
{
	return a - SSAVec4i(b);
}

SSAVec4i operator*(const SSAVec4i &a, int b)
{
	return a * SSAVec4i(b);
}

SSAVec4i operator/(const SSAVec4i &a, int b)
{
	return a / SSAVec4i(b);
}

SSAVec4i operator<<(const SSAVec4i &a, int bits)
{
	//return SSAScope::builder().CreateShl(a.v, bits);
	llvm::Value *values[2] = { a.v, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)bits)) };
	return SSAVec4i::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::x86_sse2_pslli_d), values, SSAScope::hint()));
}

SSAVec4i operator>>(const SSAVec4i &a, int bits)
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateLShr(a.v, bits, SSAScope::hint()));
}
