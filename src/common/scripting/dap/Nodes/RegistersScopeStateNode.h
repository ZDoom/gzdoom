#pragma once

#include <common/scripting/dap/GameInterfaces.h>
#include <dap/protocol.h>

#include "StateNodeBase.h"

namespace DebugServer
{

class RegistersNode : public StateNodeBase, public IProtocolVariableSerializable, public IStructuredState
{
	protected:
	VMFrame *m_stackFrame;
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	std::string m_name;
	public:
	RegistersNode(std::string name, VMFrame *stackFrame);

	virtual int GetNumberOfRegisters() = 0;

	virtual VMValue GetRegisterValue(int index) = 0;

	virtual PType *GetRegisterType([[maybe_unused]] int index) = 0;

	bool SerializeToProtocol(dap::Variable &variable) override;

	bool GetChildNames(std::vector<std::string> &names) override;

	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};

class PointerRegistersNode : public RegistersNode
{
	public:
	int GetNumberOfRegisters() override;

	VMValue GetRegisterValue(int index) override;

	PType *GetRegisterType([[maybe_unused]] int index) override;

	PointerRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };

	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node);
};

class StringRegistersNode : public RegistersNode
{
	public:
	int GetNumberOfRegisters() override;

	VMValue GetRegisterValue(int index) override;

	PType *GetRegisterType([[maybe_unused]] int index) override;

	StringRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };
};

class FloatRegistersNode : public RegistersNode
{
	public:
	int GetNumberOfRegisters() override;

	VMValue GetRegisterValue(int index) override;

	PType *GetRegisterType([[maybe_unused]] int index) override;

	FloatRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };
};

class IntRegistersNode : public RegistersNode
{
	public:
	int GetNumberOfRegisters() override;

	VMValue GetRegisterValue(int index) override;

	PType *GetRegisterType([[maybe_unused]] int index) override;

	IntRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };
};

class ParamsRegistersNode : public RegistersNode
{
	public:
	int GetNumberOfRegisters() override;

	VMValue GetRegisterValue(int index) override;

	PType *GetRegisterType([[maybe_unused]] int index) override;

	ParamsRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };

	bool SerializeToProtocol(dap::Variable &variable) override;
};

class RegistersScopeStateNode : public StateNodeBase, public IProtocolScopeSerializable, public IStructuredState
{
	VMFrame *m_stackFrame;
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	public:
	RegistersScopeStateNode(VMFrame *stackFrame);

	bool SerializeToProtocol(dap::Scope &scope) override;

	bool GetChildNames(std::vector<std::string> &names) override;

	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
