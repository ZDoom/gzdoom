#pragma once
#include <common/scripting/dap/GameInterfaces.h>

#include <dap/protocol.h>
#include "StateNodeBase.h"

namespace DebugServer
{
class StructStateNode : public StateNodeBase, public IProtocolVariableSerializable, public IStructuredState
{
	std::string m_name;
	StructInfo m_structInfo;
	const VMValue m_value;
	PType *m_type;
	const VMFrame * m_currentFrame = nullptr;
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	void CacheState();
	public:
	StructStateNode(std::string name, const VMValue value, PType *knownType,const VMFrame * currentFrame = nullptr);

	bool SerializeToProtocol(dap::Variable &variable) override;

	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
