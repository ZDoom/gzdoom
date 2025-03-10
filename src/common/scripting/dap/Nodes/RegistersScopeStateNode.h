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

			virtual std::string GetPrefix() const = 0;
			virtual int GetNumberOfRegisters() const = 0;

			virtual VMValue GetRegisterValue(int index) const = 0;

			virtual PType *GetRegisterType([[maybe_unused]] int index) const = 0;

			bool SerializeToProtocol(dap::Variable &variable) override;

			bool GetChildNames(std::vector<std::string> &names) override;

			bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};

class PointerRegistersNode : public RegistersNode
{
	public:
	int GetNumberOfRegisters() const override;

			std::string GetPrefix() const override { return "a"; }
			VMValue GetRegisterValue(int index) const override;

			PType *GetRegisterType([[maybe_unused]] int index) const override;

			PointerRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };

			bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node);
};

class StringRegistersNode : public RegistersNode
{
	public:
	std::string GetPrefix() const override { return "s"; }
	int GetNumberOfRegisters() const override;

	VMValue GetRegisterValue(int index) const override;

	PType *GetRegisterType([[maybe_unused]] int index) const override;

	StringRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };
};

class FloatRegistersNode : public RegistersNode
{
	public:
	std::string GetPrefix() const override { return "f"; }

			int GetNumberOfRegisters() const override;

	VMValue GetRegisterValue(int index) const override;

	PType *GetRegisterType([[maybe_unused]] int index) const override;

	FloatRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };
};

class IntRegistersNode : public RegistersNode
{
	public:
	std::string GetPrefix() const override { return "d"; }

			int GetNumberOfRegisters() const override;

	VMValue GetRegisterValue(int index) const override;

	PType *GetRegisterType([[maybe_unused]] int index) const override;

	IntRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };
};

class ParamsRegistersNode : public RegistersNode
{
public:
	std::string GetPrefix() const override { return ""; }

		int GetNumberOfRegisters() const override;

	VMValue GetRegisterValue(int index) const override;

	PType *GetRegisterType([[maybe_unused]] int index) const override;

	ParamsRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };

	bool SerializeToProtocol(dap::Variable &variable) override;
};

class SpecialSetupRegistersNode : public RegistersNode
{
	public:
	std::string GetPrefix() const override { return ""; }

	int GetNumberOfRegisters() const override;

	VMValue GetRegisterValue(int index) const override;

	PType *GetRegisterType([[maybe_unused]] int index) const override;

	SpecialSetupRegistersNode(std::string name, VMFrame *stackFrame) : RegistersNode(name, stackFrame) { };

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
