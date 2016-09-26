
#pragma once

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAInt;

class SSAFloat
{
public:
	SSAFloat();
	SSAFloat(SSAInt i);
	SSAFloat(float constant);
	explicit SSAFloat(llvm::Value *v);
	static SSAFloat from_llvm(llvm::Value *v) { return SSAFloat(v); }
	static llvm::Type *llvm_type();
	static SSAFloat sqrt(SSAFloat f);
	static SSAFloat sin(SSAFloat val);
	static SSAFloat cos(SSAFloat val);
	static SSAFloat pow(SSAFloat val, SSAFloat power);
	static SSAFloat exp(SSAFloat val);
	static SSAFloat log(SSAFloat val);
	static SSAFloat fma(SSAFloat a, SSAFloat b, SSAFloat c);

	llvm::Value *v;
};

SSAFloat operator+(const SSAFloat &a, const SSAFloat &b);
SSAFloat operator-(const SSAFloat &a, const SSAFloat &b);
SSAFloat operator*(const SSAFloat &a, const SSAFloat &b);
SSAFloat operator/(const SSAFloat &a, const SSAFloat &b);

SSAFloat operator+(float a, const SSAFloat &b);
SSAFloat operator-(float a, const SSAFloat &b);
SSAFloat operator*(float a, const SSAFloat &b);
SSAFloat operator/(float a, const SSAFloat &b);

SSAFloat operator+(const SSAFloat &a, float b);
SSAFloat operator-(const SSAFloat &a, float b);
SSAFloat operator*(const SSAFloat &a, float b);
SSAFloat operator/(const SSAFloat &a, float b);
