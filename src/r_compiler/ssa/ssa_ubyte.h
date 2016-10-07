
#pragma once

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAInt;

class SSAUByte
{
public:
	SSAUByte();
	explicit SSAUByte(unsigned char constant);
	explicit SSAUByte(llvm::Value *v);
	static SSAUByte from_llvm(llvm::Value *v) { return SSAUByte(v); }
	static llvm::Type *llvm_type();

	SSAInt zext_int();

	llvm::Value *v;
};

SSAUByte operator+(const SSAUByte &a, const SSAUByte &b);
SSAUByte operator-(const SSAUByte &a, const SSAUByte &b);
SSAUByte operator*(const SSAUByte &a, const SSAUByte &b);
//SSAUByte operator/(const SSAUByte &a, const SSAUByte &b);

SSAUByte operator+(unsigned char a, const SSAUByte &b);
SSAUByte operator-(unsigned char a, const SSAUByte &b);
SSAUByte operator*(unsigned char a, const SSAUByte &b);
//SSAUByte operator/(unsigned char a, const SSAUByte &b);

SSAUByte operator+(const SSAUByte &a, unsigned char b);
SSAUByte operator-(const SSAUByte &a, unsigned char b);
SSAUByte operator*(const SSAUByte &a, unsigned char b);
//SSAUByte operator/(const SSAUByte &a, unsigned char b);

SSAUByte operator<<(const SSAUByte &a, unsigned char bits);
SSAUByte operator>>(const SSAUByte &a, unsigned char bits);
