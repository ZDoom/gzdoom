
#include "r_compiler/llvm_include.h"
#include "ssa_float.h"
#include "ssa_int.h"
#include "ssa_scope.h"

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

