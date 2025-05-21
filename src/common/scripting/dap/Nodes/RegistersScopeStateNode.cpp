#include "RegistersScopeStateNode.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>

static const char *const PARAMS = "Params";
static const char *const INTS = "Ints";
static const char *const FLOATS = "Floats";
static const char *const STRINGS = "Strings";
static const char *const POINTERS = "Pointers";
static const char *const SPECIAL_SETUP = "SpecialInits";
namespace DebugServer
{
RegistersScopeStateNode::RegistersScopeStateNode(VMFrame *stackFrame) : m_stackFrame(stackFrame) { }

bool RegistersScopeStateNode::SerializeToProtocol(dap::Scope &scope)
{
	scope.name = "Registers";
	scope.expensive = false;
	scope.presentationHint = "registers";
	scope.variablesReference = GetId();

	std::vector<std::string> childNames;
	GetChildNames(childNames);

	scope.namedVariables = childNames.size();
	scope.indexedVariables = 0;

	return true;
}

bool RegistersScopeStateNode::GetChildNames(std::vector<std::string> &names)
{
	names.emplace_back(PARAMS);
	names.emplace_back(INTS);
	names.emplace_back(FLOATS);
	names.emplace_back(STRINGS);
	names.emplace_back(POINTERS);
	if (GetVMScriptFunction(m_stackFrame->Func))
	{
		names.emplace_back(SPECIAL_SETUP);
	}
	return true;
}

bool RegistersScopeStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (CaseInsensitiveEquals(name, PARAMS))
	{
		node = std::make_shared<ParamsRegistersNode>(name, m_stackFrame);
		return true;
	}
	else if (CaseInsensitiveEquals(name, INTS))
	{
		node = std::make_shared<IntRegistersNode>(name, m_stackFrame);
		return true;
	}
	else if (CaseInsensitiveEquals(name, FLOATS))
	{
		node = std::make_shared<FloatRegistersNode>(name, m_stackFrame);
		return true;
	}
	else if (CaseInsensitiveEquals(name, STRINGS))
	{
		node = std::make_shared<StringRegistersNode>(name, m_stackFrame);
		return true;
	}
	else if (CaseInsensitiveEquals(name, POINTERS))
	{
		node = std::make_shared<PointerRegistersNode>(name, m_stackFrame);
		return true;
	}
	else if (CaseInsensitiveEquals(name, SPECIAL_SETUP))
	{
		node = std::make_shared<SpecialSetupRegistersNode>(name, m_stackFrame);
		return true;
	}

	return false;
}

bool RegistersNode::SerializeToProtocol(dap::Variable &variable)
{

	variable.name = m_name;
	variable.type = m_name + " Registers";
	// value will be the max number of registers
	auto max_num_reg = GetNumberOfRegisters();
	variable.value = m_name + "[" + std::to_string(max_num_reg) + "]";
	variable.namedVariables = max_num_reg;
	variable.variablesReference = GetId();
	return true;
}

bool RegistersNode::GetChildNames(std::vector<std::string> &names)
{
	for (int i = 0; i < GetNumberOfRegisters(); i++)
	{
		names.push_back(GetPrefix() + std::to_string(i));
	}
	return true;
}

bool RegistersNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{

	// name is "a2" or "s3" etc
	std::string prefix = GetPrefix();
	int index = std::stoi(name.substr(prefix.size()));
	if (index < 0 || index >= GetNumberOfRegisters())
	{
		return false;
	}
	VMValue val = GetRegisterValue(index);
	node = RuntimeState::CreateNodeForVariable(name, val, GetRegisterType(index));
	return true;
}

int PointerRegistersNode::GetNumberOfRegisters() const
{
	return m_stackFrame->NumRegA;
}

VMValue PointerRegistersNode::GetRegisterValue(int index) const
{
	return m_stackFrame->GetRegA()[index];
}

PType *PointerRegistersNode::GetRegisterType(int index) const
{
	return TypeVoidPtr;
}

int StringRegistersNode::GetNumberOfRegisters() const
{
	return m_stackFrame->NumRegS;
}

VMValue StringRegistersNode::GetRegisterValue(int index) const
{
	return {&m_stackFrame->GetRegS()[index]};
}

PType *StringRegistersNode::GetRegisterType(int index) const
{
	return TypeString;
}

int FloatRegistersNode::GetNumberOfRegisters() const
{
	return m_stackFrame->NumRegF;
}

VMValue FloatRegistersNode::GetRegisterValue(int index) const
{
	return m_stackFrame->GetRegF()[index];
}

PType *FloatRegistersNode::GetRegisterType(int index) const
{
	return TypeFloat64;
}

int IntRegistersNode::GetNumberOfRegisters() const
{
	return m_stackFrame->NumRegD;
}

VMValue IntRegistersNode::GetRegisterValue(int index) const
{
	return m_stackFrame->GetRegD()[index];
}

PType *IntRegistersNode::GetRegisterType(int index) const
{
	return TypeSInt32;
}

int ParamsRegistersNode::GetNumberOfRegisters() const
{
	return m_stackFrame->MaxParam;
}

VMValue ParamsRegistersNode::GetRegisterValue(int index) const
{
	return m_stackFrame->GetParam()[index];
}

PType *ParamsRegistersNode::GetRegisterType(int index) const
{
	// TODO: Is it possible to get the type of parameters?
	return TypeVoidPtr;
}

bool PointerRegistersNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	std::string prefix = GetPrefix();
	int index = std::stoi(name.substr(prefix.size()));
	if (index < 0 || index >= GetNumberOfRegisters())
	{
		return false;
	}
	VMValue val = GetRegisterValue(index);
	node = RuntimeState::CreateNodeForVariable(name, val, GetRegisterType(index));
	return true;
}

bool ParamsRegistersNode::SerializeToProtocol(dap::Variable &variable)
{

	variable.name = PARAMS;
	variable.type = "Parameter Registers";
	// value will be the max number of registers
	auto max_num_reg = GetNumberOfRegisters();
	variable.value = "Params - Max: " + std::to_string(max_num_reg) + ", In Use: " + std::to_string(m_stackFrame->NumParam);
	variable.indexedVariables = max_num_reg;
	variable.variablesReference = GetId();
	return true;
}

RegistersNode::RegistersNode(std::string name, VMFrame *stackFrame) : m_stackFrame(stackFrame), m_name(name) { }

//SpecialSetupRegistersNode

bool SpecialSetupRegistersNode::SerializeToProtocol(dap::Variable &variable)
{

	variable.name = SPECIAL_SETUP;
	variable.type = "Special Setup Registers";
	// value will be the max number of registers
	auto max_num_reg = GetNumberOfRegisters();
	variable.value = "Special Setup - Max: " + std::to_string(max_num_reg);
	variable.indexedVariables = max_num_reg;
	variable.variablesReference = GetId();
	return true;
}

int SpecialSetupRegistersNode::GetNumberOfRegisters() const
{
	return GetVMScriptFunction(m_stackFrame->Func)->SpecialInits.size();
}

VMValue SpecialSetupRegistersNode::GetRegisterValue(int index) const
{
	auto *fun = GetVMScriptFunction(m_stackFrame->Func);
	auto *addr = m_stackFrame->GetExtra();
	auto *caddr = static_cast<char *>(addr);
	auto &tao = fun->SpecialInits[index];
	auto *type = tao.first;
	void *var = caddr + tao.second;
	return GetVMValue(var, type);
}

PType *SpecialSetupRegistersNode::GetRegisterType(int index) const
{
	auto *fun = GetVMScriptFunction(m_stackFrame->Func);
	auto &tao = fun->SpecialInits[index];
	return const_cast<PType *>(tao.first);
}


} // namespace DebugServer
