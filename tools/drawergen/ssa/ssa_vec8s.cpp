/*
**  SSA vec8 int16
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
#include "ssa_vec8s.h"
#include "ssa_vec4i.h"
#include "ssa_vec16ub.h"
#include "ssa_scope.h"

SSAVec8s::SSAVec8s()
: v(0)
{
}

SSAVec8s::SSAVec8s(short constant)
: v(0)
{
	std::vector<llvm::Constant*> constants;
	constants.resize(8, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant, true)));
	v = llvm::ConstantVector::get(constants);
}

SSAVec8s::SSAVec8s(short constant0, short constant1, short constant2, short constant3, short constant4, short constant5, short constant6, short constant7)
: v(0)
{
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant0, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant1, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant2, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant3, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant4, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant5, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant6, true)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant7, true)));
	v = llvm::ConstantVector::get(constants);
}

SSAVec8s::SSAVec8s(llvm::Value *v)
: v(v)
{
}

SSAVec8s::SSAVec8s(SSAVec4i i0, SSAVec4i i1)
: v(0)
{
#ifdef ARM_TARGET
	/*
	llvm::Value *int16x4_i0 = SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::arm_neon_vqmovns), i0.v, SSAScope::hint());
	llvm::Value *int16x4_i1 = SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::arm_neon_vqmovns), i1.v, SSAScope::hint());
	v = shuffle(from_llvm(int16x4_i0), from_llvm(int16x4_i1), 0, 1, 2, 3, 4, 5, 6, 7).v;
	*/
	// To do: add some clamping here
	llvm::Value *int16x4_i0 = SSAScope::builder().CreateTrunc(i0.v, llvm::VectorType::get(llvm::Type::getInt16Ty(SSAScope::context()), 4));
	llvm::Value *int16x4_i1 = SSAScope::builder().CreateTrunc(i1.v, llvm::VectorType::get(llvm::Type::getInt16Ty(SSAScope::context()), 4));
	v = shuffle(from_llvm(int16x4_i0), from_llvm(int16x4_i1), 0, 1, 2, 3, 4, 5, 6, 7).v;
#else
	llvm::Value *values[2] = { i0.v, i1.v };
	v = SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::x86_sse2_packssdw_128), values, SSAScope::hint());
#endif
}

llvm::Type *SSAVec8s::llvm_type()
{
	return llvm::VectorType::get(llvm::Type::getInt16Ty(SSAScope::context()), 8);
}

SSAVec8s SSAVec8s::bitcast(SSAVec16ub i8)
{
	return SSAVec8s::from_llvm(SSAScope::builder().CreateBitCast(i8.v, llvm_type(), SSAScope::hint()));
}

SSAVec8s SSAVec8s::shuffle(const SSAVec8s &i0, int index0, int index1, int index2, int index3, int index4, int index5, int index6, int index7)
{
	return shuffle(i0, from_llvm(llvm::UndefValue::get(llvm_type())), index0, index1, index2, index3, index4, index5, index6, index7);
}

SSAVec8s SSAVec8s::shuffle(const SSAVec8s &i0, const SSAVec8s &i1, int index0, int index1, int index2, int index3, int index4, int index5, int index6, int index7)
{
	std::vector<llvm::Constant*> constants;
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index0)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index1)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index2)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index3)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index4)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index5)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index6)));
	constants.push_back(llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, index7)));
	llvm::Value *mask = llvm::ConstantVector::get(constants);
	return SSAVec8s::from_llvm(SSAScope::builder().CreateShuffleVector(i0.v, i1.v, mask, SSAScope::hint()));
}

SSAVec8s SSAVec8s::extendhi(SSAVec16ub a)
{
	return SSAVec8s::bitcast(SSAVec16ub::shuffle(a, SSAVec16ub((unsigned char)0), 8, 16+8, 9, 16+9, 10, 16+10, 11, 16+11, 12, 16+12, 13, 16+13, 14, 16+14, 15, 16+15)); // _mm_unpackhi_epi8
}

SSAVec8s SSAVec8s::extendlo(SSAVec16ub a)
{
	return SSAVec8s::bitcast(SSAVec16ub::shuffle(a, SSAVec16ub((unsigned char)0), 0, 16+0, 1, 16+1, 2, 16+2, 3, 16+3, 4, 16+4, 5, 16+5, 6, 16+6, 7, 16+7)); // _mm_unpacklo_epi8
}

SSAVec8s operator+(const SSAVec8s &a, const SSAVec8s &b)
{
	return SSAVec8s::from_llvm(SSAScope::builder().CreateAdd(a.v, b.v, SSAScope::hint()));
}

SSAVec8s operator-(const SSAVec8s &a, const SSAVec8s &b)
{
	return SSAVec8s::from_llvm(SSAScope::builder().CreateSub(a.v, b.v, SSAScope::hint()));
}

SSAVec8s operator*(const SSAVec8s &a, const SSAVec8s &b)
{
	return SSAVec8s::from_llvm(SSAScope::builder().CreateMul(a.v, b.v, SSAScope::hint()));
}

SSAVec8s operator/(const SSAVec8s &a, const SSAVec8s &b)
{
	return SSAVec8s::from_llvm(SSAScope::builder().CreateSDiv(a.v, b.v, SSAScope::hint()));
}

SSAVec8s operator+(short a, const SSAVec8s &b)
{
	return SSAVec8s(a) + b;
}

SSAVec8s operator-(short a, const SSAVec8s &b)
{
	return SSAVec8s(a) - b;
}

SSAVec8s operator*(short a, const SSAVec8s &b)
{
	return SSAVec8s(a) * b;
}

SSAVec8s operator/(short a, const SSAVec8s &b)
{
	return SSAVec8s(a) / b;
}

SSAVec8s operator+(const SSAVec8s &a, short b)
{
	return a + SSAVec8s(b);
}

SSAVec8s operator-(const SSAVec8s &a, short b)
{
	return a - SSAVec8s(b);
}

SSAVec8s operator*(const SSAVec8s &a, short b)
{
	return a * SSAVec8s(b);
}

SSAVec8s operator/(const SSAVec8s &a, short b)
{
	return a / SSAVec8s(b);
}

SSAVec8s operator<<(const SSAVec8s &a, int bits)
{
	return SSAVec8s::from_llvm(SSAScope::builder().CreateShl(a.v, bits, SSAScope::hint()));
}

SSAVec8s operator>>(const SSAVec8s &a, int bits)
{
	return SSAVec8s::from_llvm(SSAScope::builder().CreateLShr(a.v, bits, SSAScope::hint()));
}
