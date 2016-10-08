
#pragma once

#include "ssa_int.h"
#include "ssa_vec4f.h"

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAVec4fPtr
{
public:
	SSAVec4fPtr();
	explicit SSAVec4fPtr(llvm::Value *v);
	static SSAVec4fPtr from_llvm(llvm::Value *v) { return SSAVec4fPtr(v); }
	static llvm::Type *llvm_type();
	SSAVec4fPtr operator[](SSAInt index) const;
	SSAVec4f load(bool constantScopeDomain) const;
	SSAVec4f load_unaligned(bool constantScopeDomain) const;
	void store(const SSAVec4f &new_value);
	void store_unaligned(const SSAVec4f &new_value);

	llvm::Value *v;
};
