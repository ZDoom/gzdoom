
#pragma once

#include "ssa_float.h"
#include "ssa_int.h"
#include "ssa_vec4i.h"

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAIntPtr
{
public:
	SSAIntPtr();
	explicit SSAIntPtr(llvm::Value *v);
	static SSAIntPtr from_llvm(llvm::Value *v) { return SSAIntPtr(v); }
	static llvm::Type *llvm_type();
	SSAIntPtr operator[](SSAInt index) const;
	SSAInt load() const;
	SSAVec4i load_vec4i() const;
	SSAVec4i load_unaligned_vec4i() const;
	void store(const SSAInt &new_value);
	void store_vec4i(const SSAVec4i &new_value);
	void store_unaligned_vec4i(const SSAVec4i &new_value);

	llvm::Value *v;
};
