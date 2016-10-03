
#pragma once

#include "ssa_int.h"
#include "ssa_float.h"

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSABool
{
public:
	SSABool();
	//SSABool(bool constant);
	explicit SSABool(llvm::Value *v);
	static SSABool from_llvm(llvm::Value *v) { return SSABool(v); }
	static llvm::Type *llvm_type();

	llvm::Value *v;
};

SSABool operator&&(const SSABool &a, const SSABool &b);
SSABool operator||(const SSABool &a, const SSABool &b);

SSABool operator!(const SSABool &a);

SSABool operator<(const SSAInt &a, const SSAInt &b);
SSABool operator<=(const SSAInt &a, const SSAInt &b);
SSABool operator==(const SSAInt &a, const SSAInt &b);
SSABool operator>=(const SSAInt &a, const SSAInt &b);
SSABool operator>(const SSAInt &a, const SSAInt &b);

SSABool operator<(const SSAFloat &a, const SSAFloat &b);
SSABool operator<=(const SSAFloat &a, const SSAFloat &b);
SSABool operator==(const SSAFloat &a, const SSAFloat &b);
SSABool operator>=(const SSAFloat &a, const SSAFloat &b);
SSABool operator>(const SSAFloat &a, const SSAFloat &b);
