
#pragma once

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAVec4f;
class SSAVec8s;
class SSAVec16ub;
class SSAInt;

class SSAVec4i
{
public:
	SSAVec4i();
	SSAVec4i(int constant);
	SSAVec4i(int constant0, int constant1, int constant2, int constant3);
	SSAVec4i(SSAInt i);
	SSAVec4i(SSAInt i0, SSAInt i1, SSAInt i2, SSAInt i3);
	explicit SSAVec4i(llvm::Value *v);
	SSAVec4i(SSAVec4f f32);
	SSAInt operator[](SSAInt index);
	SSAVec4i insert(SSAInt index, SSAInt value);
	SSAVec4i insert(int index, SSAInt value);
	static SSAVec4i bitcast(SSAVec4f f32);
	static SSAVec4i bitcast(SSAVec8s i16);
	static SSAVec4i shuffle(const SSAVec4i &f0, int index0, int index1, int index2, int index3);
	static SSAVec4i shuffle(const SSAVec4i &f0, const SSAVec4i &f1, int index0, int index1, int index2, int index3);
	static SSAVec4i extendhi(SSAVec8s i16);
	static SSAVec4i extendlo(SSAVec8s i16);
	static void extend(SSAVec16ub a, SSAVec4i &out0, SSAVec4i &out1, SSAVec4i &out2, SSAVec4i &out3);
	static SSAVec4i combinehi(SSAVec8s v0, SSAVec8s v1);
	static SSAVec4i combinelo(SSAVec8s v0, SSAVec8s v1);
	static SSAVec4i sqrt(SSAVec4i f);
	//static SSAVec4i min_sse41(SSAVec4i a, SSAVec4i b);
	//static SSAVec4i max_sse41(SSAVec4i a, SSAVec4i b);
	static SSAVec4i from_llvm(llvm::Value *v) { return SSAVec4i(v); }
	static llvm::Type *llvm_type();

	llvm::Value *v;
};

SSAVec4i operator+(const SSAVec4i &a, const SSAVec4i &b);
SSAVec4i operator-(const SSAVec4i &a, const SSAVec4i &b);
SSAVec4i operator*(const SSAVec4i &a, const SSAVec4i &b);
SSAVec4i operator/(const SSAVec4i &a, const SSAVec4i &b);

SSAVec4i operator+(int a, const SSAVec4i &b);
SSAVec4i operator-(int a, const SSAVec4i &b);
SSAVec4i operator*(int a, const SSAVec4i &b);
SSAVec4i operator/(int a, const SSAVec4i &b);

SSAVec4i operator+(const SSAVec4i &a, int b);
SSAVec4i operator-(const SSAVec4i &a, int b);
SSAVec4i operator*(const SSAVec4i &a, int b);
SSAVec4i operator/(const SSAVec4i &a, int b);

SSAVec4i operator<<(const SSAVec4i &a, int bits);
SSAVec4i operator>>(const SSAVec4i &a, int bits);
