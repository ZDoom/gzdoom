
#pragma once

#include <vector>

namespace llvm { class Value; }

class SSAInt;
class SSAIndexLookup;

class SSAValue
{
public:
	SSAValue() : v(0) { }

	static SSAValue from_llvm(llvm::Value *v) { SSAValue val; val.v = v; return val; }

	SSAValue load(bool constantScopeDomain);
	void store(llvm::Value *v);

	template<typename Type>
	operator Type()
	{
		return Type::from_llvm(v);
	}

	SSAIndexLookup operator[](int index);
	SSAIndexLookup operator[](SSAInt index);

	llvm::Value *v;
};

class SSAIndexLookup
{
public:
	SSAIndexLookup() : v(0) { }

	llvm::Value *v;
	std::vector<llvm::Value *> indexes;

	SSAValue load(bool constantScopeDomain) { SSAValue value = *this; return value.load(constantScopeDomain); }
	void store(llvm::Value *v) { SSAValue value = *this; return value.store(v); }

	template<typename Type>
	operator Type()
	{
		return Type::from_llvm(v);
	}

	operator SSAValue();
	SSAIndexLookup operator[](int index);
	SSAIndexLookup operator[](SSAInt index);
};
