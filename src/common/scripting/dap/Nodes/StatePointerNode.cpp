#include "StatePointerNode.h"

#include "types.h"
#include "ValueStateNode.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>
#include <common/objects/dobject.h>
#include <common/scripting/core/symbols.h>
#include <info.h>
#include "DummyNode.h"
static const FString invalidString = FString("<INVALID>");

namespace DebugServer
{
StatePointerNode::StatePointerNode(std::string name, VMValue value, PStatePointer *knownType) : m_name(name), m_value(value), m_type(knownType) { }

bool StatePointerNode::SerializeToProtocol(dap::Variable &variable)
{
	variable.variablesReference = IsVMValueValid(&m_value) ? GetId() : 0;
	variable.namedVariables = 0;
	std::vector<std::string> names;
	GetChildNames(names);
	variable.namedVariables = names.size();
	variable.name = m_name;
	variable.type = "StatePointer";
	if (!IsVMValueValid(&m_value))
	{
		variable.value = "<NULL>";
	}
	else
	{
		variable.value = FState::StaticGetStateName(static_cast<FState *>(m_value.a)).GetChars();
	}
	return true;
}

bool StatePointerNode::GetChildNames(std::vector<std::string> &names)
{

	names.push_back("sprite");
	names.push_back("Tics");
	names.push_back("TicRange");
	names.push_back("Light");
	names.push_back("StateFlags");
	names.push_back("Frame");
	names.push_back("UseFlags");
	names.push_back("DefineFlags");
	names.push_back("Misc1");
	names.push_back("Misc2");
	names.push_back("DehIndex");

	return true;
}

bool StatePointerNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (!IsVMValueValid(&m_value))
	{
		return false;
	}

	FState *state = static_cast<FState *>(m_value.a);
	if (!state)
	{
		return false;
	}
	if (name == "sprite")
	{
		node = std::make_shared<ValueStateNode>("sprite", state->sprite, TypeSpriteID);
		return true;
	}
	else if (name == "Tics")
	{
		node = std::make_shared<ValueStateNode>("Tics", state->Tics, TypeSInt16);
		return true;
	}
	else if (name == "TicRange")
	{
		node = std::make_shared<ValueStateNode>("TicRange", state->TicRange, TypeUInt16);
		return true;
	}
	else if (name == "Light")
	{
		node = std::make_shared<ValueStateNode>("Light", state->Light, TypeSInt16);
		return true;
	}
	else if (name == "StateFlags")
	{
		std::vector<std::string> strings;
		if (state->StateFlags & STF_SLOW)
		{
			strings.push_back("STF_SLOW");
		}
		if (state->StateFlags & STF_FAST)
		{
			strings.push_back("STF_FAST");
		}
		if (state->StateFlags & STF_FULLBRIGHT)
		{
			strings.push_back("STF_FULLBRIGHT");
		}
		if (state->StateFlags & STF_NODELAY)
		{
			strings.push_back("STF_NODELAY");
		}
		if (state->StateFlags & STF_SAMEFRAME)
		{
			strings.push_back("STF_SAMEFRAME");
		}
		if (state->StateFlags & STF_CANRAISE)
		{
			strings.push_back("STF_CANRAISE");
		}
		if (state->StateFlags & STF_DEHACKED)
		{
			strings.push_back("STF_DEHACKED");
		}
		if (state->StateFlags & STF_CONSUMEAMMO)
		{
			strings.push_back("STF_CONSUMEAMMO");
		}
		std::string value = StringFormat("%d (%s)", state->StateFlags, StringJoin(strings, " | ").c_str());
		node = std::make_shared<DummyNode>("StateFlags", value, "StateFlags");
		return true;
	}
	else if (name == "Frame")
	{
		node = std::make_shared<ValueStateNode>("Frame", state->Frame, TypeUInt8);
		return true;
	}
	else if (name == "UseFlags")
	{
		std::vector<std::string> strings;
		if (state->UseFlags & SUF_ACTOR)
		{
			strings.push_back("SUF_ACTOR");
		}
		if (state->UseFlags & SUF_OVERLAY)
		{
			strings.push_back("SUF_OVERLAY");
		}
		if (state->UseFlags & SUF_WEAPON)
		{
			strings.push_back("SUF_WEAPON");
		}
		if (state->UseFlags & SUF_ITEM)
		{
			strings.push_back("SUF_ITEM");
		}
		// join them together with a ' | ' separator
		const char *const delim = " | ";
		std::ostringstream imploded;
		std::copy(strings.begin(), strings.end(), std::ostream_iterator<std::string>(imploded, delim));

		node = std::make_shared<ValueStateNode>("UseFlags", state->DefineFlags, TypeUInt8);
		return true;
	}
	else if (name == "DefineFlags")
	{
		std::string value = std::to_string(state->DefineFlags) + " ";
		switch (state->DefineFlags)
		{
		case SDF_NEXT:
			value += "SDF_NEXT";
			break;
		case SDF_STATE:
			value += "SDF_STATE";
			break;
		case SDF_STOP:
			value += "SDF_STOP";
			break;
		case SDF_WAIT:
			value += "SDF_WAIT";
			break;
		case SDF_LABEL:
			value += "SDF_LABEL";
			break;
		case SDF_INDEX:
			value += "SDF_INDEX";
			break;
		case SDF_MASK:
			value += "SDF_MASK";
			break;
		default:
			value += "<INVALID>";
			break;
		}
		node = std::make_shared<DummyNode>("DefineFlags", value, "DefineFlags");
		return true;
	}
	else if (name == "Misc1")
	{
		node = std::make_shared<ValueStateNode>("Misc1", state->Misc1, TypeSInt32);
		return true;
	}
	else if (name == "Misc2")
	{
		node = std::make_shared<ValueStateNode>("Misc2", state->Misc2, TypeSInt32);
		return true;
	}
	else if (name == "DehIndex")
	{
		node = std::make_shared<ValueStateNode>("DehIndex", state->DehIndex, TypeSInt32);
		return true;
	}

	return true;
}
}
