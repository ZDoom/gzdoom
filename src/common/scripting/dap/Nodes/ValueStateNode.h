#pragma once

#include <common/scripting/dap/GameInterfaces.h>

#include <dap/protocol.h>
#include "StateNodeBase.h"

namespace DebugServer
{
class ValueStateNode : public StateNodeNamedVariable
{
	const VMValue m_variable;
	PType *m_type;
	PClass *m_StateOwningClass = nullptr;
	public:
	ValueStateNode(std::string name, VMValue variable, PType *type, PClass *stateOwningClass = nullptr);
	bool SerializeToProtocol(dap::Variable &variable) override;
	static dap::Variable ToVariable(const VMValue &m_variable, PType *m_type, PClass *stateOwningClass = nullptr);
};
}
