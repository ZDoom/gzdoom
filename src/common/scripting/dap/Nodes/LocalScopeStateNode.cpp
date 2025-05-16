#include "LocalScopeStateNode.h"
#include "common/scripting/dap/GameInterfaces.h"
#include "types.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>

namespace DebugServer
{
static const char *const LOCAL = "Local";
static const char *const SELF = "self";
static const char *const INVOKER = "invoker";
static const char *const STATE_POINTER = "state_pointer";

LocalScopeStateNode::LocalScopeStateNode(VMFrame *stackFrame) : m_stackFrame(stackFrame) { }

bool LocalScopeStateNode::SerializeToProtocol(dap::Scope &scope)
{
	scope.name = LOCAL;
	scope.expensive = false;

	scope.variablesReference = GetId();
	scope.presentationHint = "locals";
	std::vector<std::string> childNames;
	GetChildNames(childNames);

	scope.namedVariables = childNames.size();
	scope.indexedVariables = 0;

	return true;
}

bool LocalScopeStateNode::GetChildNames(std::vector<std::string> &names)
{
	if (m_stackFrame->Func->VarFlags & VARF_Action)
	{
		names.push_back(SELF);
		names.push_back(INVOKER);
		names.push_back(STATE_POINTER);
	}
	else if (m_stackFrame->Func->VarFlags & VARF_Method)
	{
		names.push_back(SELF);
		// TODO: Figure out if there is a state_pointer in non-action methods?
	}
	// TODO: verify that a method is static (and has no members) as indicated by absence of VARF_Method on VMFunction::VarFlags

	if (IsFunctionNative(m_stackFrame->Func))
	{
		// TODO: Can't introspect locals in native functions; could potentially add args?
		return true;
	}
	// TODO: waiting for PR to add local/register mapping to VMFrame

	return true;
}

bool LocalScopeStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (m_children.find(name) != m_children.end())
	{
		node = m_children[name];
		return true;
	}
	int paramidx = -1;
	if (CaseInsensitiveEquals(name, SELF))
	{
		paramidx = 0;
	}
	else if (CaseInsensitiveEquals(name, INVOKER))
	{
		paramidx = 1;
	}
	else if (CaseInsensitiveEquals(name, STATE_POINTER))
	{
		paramidx = 1;
		if (m_stackFrame->Func->VarFlags & VARF_Action)
		{
			paramidx = 2;
		}
	}
	if (paramidx >= 0)
	{
		if (m_stackFrame->NumRegA == 0)
		{
			LogError("Function %s has '%s' but no parameters", m_stackFrame->Func->QualifiedName, name.c_str());
			return false;
		}
		else if (m_stackFrame->NumRegA <= paramidx)
		{
			LogError("Function %s has '%s' but <= %d parameters", m_stackFrame->Func->QualifiedName, name.c_str(), paramidx);
			return false;
		}
		if (m_stackFrame->Func->Proto->ArgumentTypes.size() == 0)
		{
			LogError("Function %s has '%s' but no argument types", m_stackFrame->Func->QualifiedName, name.c_str());
			return false;
		}
		else if (m_stackFrame->Func->Proto->ArgumentTypes.size() <= paramidx)
		{
			LogError("Function %s has '%s' but <= %d argument types", m_stackFrame->Func->QualifiedName, name.c_str(), paramidx);
			return false;
		}
		auto type = m_stackFrame->Func->Proto->ArgumentTypes[paramidx];
		auto val = m_stackFrame->GetRegA()[paramidx];
		m_children[name] = RuntimeState::CreateNodeForVariable(ToLowerCopy(name), val, type);
		node = m_children[name];
		return true;
	}
	// TODO: no locals yet; waiting on PR to add local/register mapping to VMFrame
	return false;
}
}
