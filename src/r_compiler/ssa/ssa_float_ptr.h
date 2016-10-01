
#pragma once

#include "ssa_float.h"
#include "ssa_int.h"
#include "ssa_vec4f.h"

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAFloatPtr
{
public:
	SSAFloatPtr();
	explicit SSAFloatPtr(llvm::Value *v);
	static SSAFloatPtr from_llvm(llvm::Value *v) { return SSAFloatPtr(v); }
	static llvm::Type *llvm_type();
	SSAFloatPtr operator[](SSAInt index) const;
	SSAFloatPtr operator[](int index) const { return (*this)[SSAInt(index)]; }
	SSAFloat load() const;
	SSAVec4f load_vec4f() const;
	SSAVec4f load_unaligned_vec4f() const;
	void store(const SSAFloat &new_value);
	void store_vec4f(const SSAVec4f &new_value);
	void store_unaligned_vec4f(const SSAVec4f &new_value);

	llvm::Value *v;
};
