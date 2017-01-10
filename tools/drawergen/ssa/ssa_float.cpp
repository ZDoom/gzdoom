/*
**  SSA float32
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
#include "ssa_float.h"
#include "ssa_int.h"
#include "ssa_scope.h"
#include "ssa_bool.h"
#include "ssa_vec4f.h"

SSAFloat::SSAFloat()
: v(0)
{
}

SSAFloat::SSAFloat(float constant)
: v(0)
{
	v = llvm::ConstantFP::get(SSAScope::context(), llvm::APFloat(constant));
}

SSAFloat::SSAFloat(SSAInt i)
: v(0)
{
	v = SSAScope::builder().CreateSIToFP(i.v, llvm::Type::getFloatTy(SSAScope::context()), SSAScope::hint());
}

SSAFloat::SSAFloat(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAFloat::llvm_type()
{
	return llvm::Type::getFloatTy(SSAScope::context());
}

SSAFloat SSAFloat::rsqrt(SSAFloat f)
{
#ifdef ARM_TARGET
	//return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::aarch64_neon_frsqrts), f.v, SSAScope::hint()));
	return SSAFloat(1.0f) / (f * SSAFloat(0.01f));
#else
	llvm::Value *f_ss = SSAScope::builder().CreateInsertElement(llvm::UndefValue::get(SSAVec4f::llvm_type()), f.v, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)0)));
	f_ss = SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::x86_sse_rsqrt_ss), f_ss, SSAScope::hint());
	return SSAFloat::from_llvm(SSAScope::builder().CreateExtractElement(f_ss, SSAInt(0).v, SSAScope::hint()));
#endif
}

SSAFloat SSAFloat::MIN(SSAFloat a, SSAFloat b)
{
	return SSAFloat::from_llvm(SSAScope::builder().CreateSelect((a < b).v, a.v, b.v, SSAScope::hint()));
}

SSAFloat SSAFloat::MAX(SSAFloat a, SSAFloat b)
{
	return SSAFloat::from_llvm(SSAScope::builder().CreateSelect((a > b).v, a.v, b.v, SSAScope::hint()));
}

SSAFloat SSAFloat::clamp(SSAFloat a, SSAFloat b, SSAFloat c)
{
	return SSAFloat::MAX(SSAFloat::MIN(a, c), b);
}

SSAFloat operator+(const SSAFloat &a, const SSAFloat &b)
{
	return SSAFloat::from_llvm(SSAScope::builder().CreateFAdd(a.v, b.v, SSAScope::hint()));
}

SSAFloat operator-(const SSAFloat &a, const SSAFloat &b)
{
	return SSAFloat::from_llvm(SSAScope::builder().CreateFSub(a.v, b.v, SSAScope::hint()));
}

SSAFloat operator*(const SSAFloat &a, const SSAFloat &b)
{
	return SSAFloat::from_llvm(SSAScope::builder().CreateFMul(a.v, b.v, SSAScope::hint()));
}

SSAFloat operator/(const SSAFloat &a, const SSAFloat &b)
{
	return SSAFloat::from_llvm(SSAScope::builder().CreateFDiv(a.v, b.v, SSAScope::hint()));
}

SSAFloat operator+(float a, const SSAFloat &b)
{
	return SSAFloat(a) + b;
}

SSAFloat operator-(float a, const SSAFloat &b)
{
	return SSAFloat(a) - b;
}

SSAFloat operator*(float a, const SSAFloat &b)
{
	return SSAFloat(a) * b;
}

SSAFloat operator/(float a, const SSAFloat &b)
{
	return SSAFloat(a) / b;
}

SSAFloat operator+(const SSAFloat &a, float b)
{
	return a + SSAFloat(b);
}

SSAFloat operator-(const SSAFloat &a, float b)
{
	return a - SSAFloat(b);
}

SSAFloat operator*(const SSAFloat &a, float b)
{
	return a * SSAFloat(b);
}

SSAFloat operator/(const SSAFloat &a, float b)
{
	return a / SSAFloat(b);
}

