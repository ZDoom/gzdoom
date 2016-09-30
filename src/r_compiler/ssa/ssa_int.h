
#pragma once

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAFloat;

class SSAInt
{
public:
	SSAInt();
	SSAInt(int constant);
	SSAInt(SSAFloat f);
	explicit SSAInt(llvm::Value *v);
	static SSAInt from_llvm(llvm::Value *v) { return SSAInt(v); }
	static llvm::Type *llvm_type();

	static SSAInt MIN(SSAInt a, SSAInt b);
	static SSAInt MAX(SSAInt a, SSAInt b);

	llvm::Value *v;
};

SSAInt operator+(const SSAInt &a, const SSAInt &b);
SSAInt operator-(const SSAInt &a, const SSAInt &b);
SSAInt operator*(const SSAInt &a, const SSAInt &b);
SSAInt operator/(const SSAInt &a, const SSAInt &b);
SSAInt operator%(const SSAInt &a, const SSAInt &b);

SSAInt operator+(int a, const SSAInt &b);
SSAInt operator-(int a, const SSAInt &b);
SSAInt operator*(int a, const SSAInt &b);
SSAInt operator/(int a, const SSAInt &b);
SSAInt operator%(int a, const SSAInt &b);

SSAInt operator+(const SSAInt &a, int b);
SSAInt operator-(const SSAInt &a, int b);
SSAInt operator*(const SSAInt &a, int b);
SSAInt operator/(const SSAInt &a, int b);
SSAInt operator%(const SSAInt &a, int b);

SSAInt operator<<(const SSAInt &a, int bits);
SSAInt operator>>(const SSAInt &a, int bits);
SSAInt operator<<(const SSAInt &a, const SSAInt &bits);
SSAInt operator>>(const SSAInt &a, const SSAInt &bits);

SSAInt operator&(const SSAInt &a, int b);
SSAInt operator&(const SSAInt &a, const SSAInt &b);
SSAInt operator|(const SSAInt &a, int b);
SSAInt operator|(const SSAInt &a, const SSAInt &b);
