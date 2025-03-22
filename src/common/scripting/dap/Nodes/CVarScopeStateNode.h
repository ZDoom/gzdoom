#pragma once

#include <common/scripting/dap/GameInterfaces.h>
#include <dap/protocol.h>

#include "StateNodeBase.h"

class FBaseCVar;
namespace DebugServer
{

class CVarStateNode : public StateNodeBase, public IProtocolVariableSerializable{
	FBaseCVar *m_cvar = nullptr;
	public:
	CVarStateNode(FBaseCVar *cvar);
	bool SerializeToProtocol(dap::Variable &variable) override;
	static dap::Variable ToVariable(FBaseCVar *cvar);
};

class CVarNameComparer {
public:
	bool operator()(const std::string &a, const std::string &b) const {
		if (!a.empty() && !b.empty()) {
			// check if the first character is uppercase and the other is lowercase
			// uppercase goes last
			if (isupper(a[0]) && islower(b[0])) {
				return false;
			}
			if (islower(a[0]) && isupper(b[0])) {
				return true;
			}
		}
		return a < b;
	}
};

class CVarScopeStateNode  : public StateNodeBase, public IProtocolScopeSerializable, public IStructuredState
{
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	std::vector<std::string> m_CachedNames; // to preserve order
public:
	CVarScopeStateNode() = default;

	bool SerializeToProtocol(dap::Scope &scope) override;
	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
} 