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

SSAFloat SSAFloat::sqrt(SSAFloat f)
{
	std::vector<llvm::Type *> params;
	params.push_back(SSAFloat::llvm_type());
	return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::sqrt, params), f.v, SSAScope::hint()));
}

SSAFloat SSAFloat::fastsqrt(SSAFloat f)
{
	return f * rsqrt(f);
}

SSAFloat SSAFloat::rsqrt(SSAFloat f)
{
	llvm::Value *f_ss = SSAScope::builder().CreateInsertElement(llvm::UndefValue::get(SSAVec4f::llvm_type()), f.v, llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, (uint64_t)0)));
	f_ss = SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::x86_sse_rsqrt_ss), f_ss, SSAScope::hint());
	return SSAFloat::from_llvm(SSAScope::builder().CreateExtractElement(f_ss, SSAInt(0).v, SSAScope::hint()));
}

SSAFloat SSAFloat::sin(SSAFloat val)
{
	std::vector<llvm::Type *> params;
	params.push_back(SSAFloat::llvm_type());
	return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::sin, params), val.v, SSAScope::hint()));
}

SSAFloat SSAFloat::cos(SSAFloat val)
{
	std::vector<llvm::Type *> params;
	params.push_back(SSAFloat::llvm_type());
	return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::cos, params), val.v, SSAScope::hint()));
}

SSAFloat SSAFloat::pow(SSAFloat val, SSAFloat power)
{
	std::vector<llvm::Type *> params;
	params.push_back(SSAFloat::llvm_type());
	//params.push_back(SSAFloat::llvm_type());
	std::vector<llvm::Value*> args;
	args.push_back(val.v);
	args.push_back(power.v);
	return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::pow, params), args, SSAScope::hint()));
}

SSAFloat SSAFloat::exp(SSAFloat val)
{
	std::vector<llvm::Type *> params;
	params.push_back(SSAFloat::llvm_type());
	return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::exp, params), val.v, SSAScope::hint()));
}

SSAFloat SSAFloat::log(SSAFloat val)
{
	std::vector<llvm::Type *> params;
	params.push_back(SSAFloat::llvm_type());
	return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::log, params), val.v, SSAScope::hint()));
}

SSAFloat SSAFloat::fma(SSAFloat a, SSAFloat b, SSAFloat c)
{
	std::vector<llvm::Type *> params;
	params.push_back(SSAFloat::llvm_type());
	//params.push_back(SSAFloat::llvm_type());
	//params.push_back(SSAFloat::llvm_type());
	std::vector<llvm::Value*> args;
	args.push_back(a.v);
	args.push_back(b.v);
	args.push_back(c.v);
	return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::fma, params), args, SSAScope::hint()));
}

/* This intrinsic isn't always available..
SSAFloat SSAFloat::round(SSAFloat val)
{
	
	std::vector<llvm::Type *> params;
	params.push_back(SSAFloat::llvm_type());
	return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::round, params), val.v, SSAScope::hint()));
}
*/

SSAFloat SSAFloat::floor(SSAFloat val)
{
	std::vector<llvm::Type *> params;
	params.push_back(SSAFloat::llvm_type());
	return SSAFloat::from_llvm(SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::floor, params), val.v, SSAScope::hint()));
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

