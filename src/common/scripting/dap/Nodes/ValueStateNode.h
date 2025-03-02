#pragma once

#include <common/scripting/dap/GameInterfaces.h>

#include <dap/protocol.h>
#include "StateNodeBase.h"

namespace DebugServer
{
class ValueStateNode : public StateNodeBase, public IProtocolVariableSerializable
{
	std::string m_name;
	const VMValue m_variable;
	PType *m_type;
	public:
	ValueStateNode(std::string name, VMValue variable, PType *type);
	bool SerializeToProtocol(dap::Variable &variable) override;
	static dap::Variable ToVariable(const VMValue &m_variable, PType *m_type);
};
}
