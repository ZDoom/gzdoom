
#pragma once

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAFloat;
class SSAInt;

class SSAShort
{
public:
	SSAShort();
	SSAShort(int constant);
	SSAShort(SSAFloat f);
	explicit SSAShort(llvm::Value *v);
	static SSAShort from_llvm(llvm::Value *v) { return SSAShort(v); }
	static llvm::Type *llvm_type();

	llvm::Value *v;
};

SSAShort operator+(const SSAShort &a, const SSAShort &b);
SSAShort operator-(const SSAShort &a, const SSAShort &b);
SSAShort operator*(const SSAShort &a, const SSAShort &b);
SSAShort operator/(const SSAShort &a, const SSAShort &b);
SSAShort operator%(const SSAShort &a, const SSAShort &b);

SSAShort operator+(int a, const SSAShort &b);
SSAShort operator-(int a, const SSAShort &b);
SSAShort operator*(int a, const SSAShort &b);
SSAShort operator/(int a, const SSAShort &b);
SSAShort operator%(int a, const SSAShort &b);

SSAShort operator+(const SSAShort &a, int b);
SSAShort operator-(const SSAShort &a, int b);
SSAShort operator*(const SSAShort &a, int b);
SSAShort operator/(const SSAShort &a, int b);
SSAShort operator%(const SSAShort &a, int b);

SSAShort operator<<(const SSAShort &a, int bits);
SSAShort operator>>(const SSAShort &a, int bits);
SSAShort operator<<(const SSAShort &a, const SSAInt &bits);
SSAShort operator>>(const SSAShort &a, const SSAInt &bits);

SSAShort operator&(const SSAShort &a, int b);
SSAShort operator&(const SSAShort &a, const SSAShort &b);
SSAShort operator|(const SSAShort &a, int b);
SSAShort operator|(const SSAShort &a, const SSAShort &b);
