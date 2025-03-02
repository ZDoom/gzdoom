#pragma once

#include <common/scripting/dap/GameInterfaces.h>

#include <dap/protocol.h>
#include "StateNodeBase.h"

namespace DebugServer
{
class StatePointerNode : public StateNodeBase, public IProtocolVariableSerializable, public IStructuredState
{
	std::string m_name;
	const VMValue m_value;
	PStatePointer *m_type;
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	public:
	StatePointerNode(std::string name, VMValue variable, PStatePointer *type);
	bool SerializeToProtocol(dap::Variable &variable) override;
	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
