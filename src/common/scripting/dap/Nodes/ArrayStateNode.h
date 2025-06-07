#pragma once
#include <common/scripting/dap/GameInterfaces.h>

#include <dap/protocol.h>
#include <map>
#include "StateNodeBase.h"

namespace DebugServer
{
class ArrayStateNode : public StateNodeNamedVariable, public IStructuredState
{

	const VMValue m_value;
	PType *m_type;
	PType *m_elementType;
	std::map<std::string, std::shared_ptr<StateNodeBase>> m_children;
	public:
	ArrayStateNode(std::string name, VMValue value, PType *type);

	virtual ~ArrayStateNode() override = default;

	bool SerializeToProtocol(dap::Variable &variable) override;

	bool GetChildNames(std::vector<std::string> &names) override;
	bool CacheChildren();
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
