#include "CVarScopeStateNode.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>
#include "common/console/c_cvars.h"
#include "gstrings.h"

namespace DebugServer
{
CVarStateNode::CVarStateNode(FBaseCVar *cvar) : m_cvar(cvar) { }

dap::Variable CVarStateNode::ToVariable(FBaseCVar *m_cvar)
{
	dap::Variable variable;
	if (!m_cvar)
	{
		variable.name = "<INVALID>";
		variable.value = "<INVALID>";
		return variable;
	}


	const char *vmvalstr = nullptr;
	const char *realTypeString = nullptr;
	ECVarType favoriteType;
	UCVarValue val = m_cvar->GetFavoriteRep(&favoriteType);
	// VMValue vmval;
	// PType *vmtype;
	// switch (type)
	// {
	// 	case CVAR_Flag:
	// 	case CVAR_Bool:
	// 		vmval = VMValue(val.Bool);
	// 		vmtype = TypeBool;
	// 		break;
	// 	case CVAR_Int:
	// 	case CVAR_Color:
	// 	case CVAR_Mask:
	// 		vmval = VMValue(val.Int);
	// 		vmtype = TypeSInt32;
	// 		break;
	// 	case CVAR_Float:
	// 		vmval = VMValue(val.Float);
	// 		vmtype = TypeFloat64;
	// 		break;
	// 	case CVAR_String:
	// 		{
	// 			vmvalstr = val.String;
	// 			vmval = VMValue(&vmvalstr);
	// 			vmtype = TypeString;
	// 			auto test = vmval.sp;
	// 			int i = 0;
	// 		}
	// 		break;
	// 	default:
	// 		break;
	// }
	variable.value = m_cvar->GetHumanString();

	switch (m_cvar->GetRealType())
	{
	// actually do these in order of the enum
	case CVAR_Bool:
		realTypeString = "CVar<Bool>";
		break;
	case CVAR_Int:
		realTypeString = "CVar<Int>";
		break;
	case CVAR_Float:
		realTypeString = "CVar<Float>";
		if (variable.value.find('.') == std::string::npos && variable.value.find('e') == std::string::npos)
		{
			variable.value += ".0";
		}
		break;
	case CVAR_String:
	{
		realTypeString = "CVar<String>";
		// If none of the values are non-numbers or non-'.', then we need to wrap it in quotes
		if (favoriteType == CVAR_String)
		{
			variable.value = StringFormat("\"%s\"", variable.value.c_str());
		}
	}
	break;
	case CVAR_Color:
		realTypeString = "CVar<Color>";
		break;
	case CVAR_Flag:
		realTypeString = "CVar<Flag>";
		break;
	case CVAR_Mask:
		realTypeString = "CVar<Mask>";
		break;
	case CVAR_Dummy:
		realTypeString = "CVar<Dummy>";
		break;
	default:
		realTypeString = "<UNKNOWN>";
		break;
	}
	const FString &description = m_cvar->GetDescription();


	if (!description.IsEmpty())
	{
		std::string_view localized = GStrings.localize(description.GetChars());
		if (!localized.empty() && localized.substr(1) != description.GetChars())
		{
			variable.type = StringFormat("%s (%s)", realTypeString, localized.data());
		}
		else
		{
			variable.type = realTypeString;
		}
	}
	else
	{
		variable.type = realTypeString;
	}
	variable.name = m_cvar->GetName();

	return variable;
}

bool CVarStateNode::SerializeToProtocol(dap::Variable &variable)
{
	variable = ToVariable(m_cvar);
	return true;
}

bool CVarScopeStateNode::SerializeToProtocol(dap::Scope &scope)
{
	scope.name = "CVars";
	scope.expensive = true;
	scope.presentationHint = "cvars";
	scope.variablesReference = GetId();

	if (m_CachedNames.empty())
	{
		std::vector<std::string> childNames;
		GetChildNames(childNames);
	}
	scope.namedVariables = m_CachedNames.size();
	scope.indexedVariables = 0;

	return true;
}

bool CVarScopeStateNode::GetChildNames(std::vector<std::string> &names)
{
	if (!m_CachedNames.empty())
	{
		names = m_CachedNames;
		return true;
	}
	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair *pair;
	m_CachedNames.reserve(cvarMap.CountUsed());
	while (it.NextPair(pair))
	{
		auto var = pair->Value;
		m_CachedNames.emplace_back(var->GetName());
		m_children[var->GetName()] = std::make_shared<CVarStateNode>(var);
	}
	std::sort(m_CachedNames.begin(), m_CachedNames.end(), CVarNameComparer());
	names = m_CachedNames;
	return true;
}

bool CVarScopeStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (m_CachedNames.empty())
	{
		std::vector<std::string> childNames;
		GetChildNames(childNames);
	}
	if (m_children.find(name) == m_children.end())
	{
		auto var = FindCVar(name.c_str(), nullptr);
		if (!var)
		{
			return false;
		}
		m_children[var->GetName()] = std::make_shared<CVarStateNode>(var);
		m_CachedNames.push_back(var->GetName());
		std::sort(m_CachedNames.begin(), m_CachedNames.end(), CVarNameComparer());
	}
	if (m_children.find(name) != m_children.end())
	{
		node = m_children[name];
		return true;
	}
	return false;
}
}