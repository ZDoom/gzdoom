
#include "r_compiler/llvm_include.h"
#include "ssa_value.h"
#include "ssa_int.h"
#include "ssa_scope.h"

SSAValue SSAValue::load()
{
	return SSAValue::from_llvm(SSAScope::builder().CreateLoad(v, false));
}

void SSAValue::store(llvm::Value *value)
{
	SSAScope::builder().CreateStore(value, v, false);
}

SSAIndexLookup SSAValue::operator[](int index)
{
	SSAIndexLookup result;
	result.v = v;
	result.indexes.push_back(SSAInt(index).v);
	return result;
}

SSAIndexLookup SSAValue::operator[](SSAInt index)
{
	SSAIndexLookup result;
	result.v = v;
	result.indexes.push_back(index.v);
	return result;
}

/////////////////////////////////////////////////////////////////////////////

SSAIndexLookup::operator SSAValue()
{
	return SSAValue::from_llvm(SSAScope::builder().CreateGEP(v, indexes));
}

SSAIndexLookup SSAIndexLookup::operator[](int index)
{
	SSAIndexLookup result;
	result.v = v;
	result.indexes = indexes;
	result.indexes.push_back(SSAInt(index).v);
	return result;
}

SSAIndexLookup SSAIndexLookup::operator[](SSAInt index)
{
	SSAIndexLookup result;
	result.v = v;
	result.indexes = indexes;
	result.indexes.push_back(index.v);
	return result;
}
