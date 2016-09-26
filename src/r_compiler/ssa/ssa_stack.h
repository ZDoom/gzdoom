
#pragma once

template<typename SSAVariable>
class SSAStack
{
public:
	SSAStack()
	: v(0)
	{
		v = SSAScope::alloca(SSAVariable::llvm_type());
	}

	SSAVariable load() const
	{
		return SSAVariable::from_llvm(SSAScope::builder().CreateLoad(v, SSAScope::hint()));
	}

	void store(const SSAVariable &new_value)
	{
		SSAScope::builder().CreateStore(new_value.v, v);
	}

	llvm::Value *v;
};
