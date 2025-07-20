#include "ValueStateNode.h"

#include "actor.h"

#include <common/scripting/dap/Utilities.h>
#include "common/scripting/dap/GameInterfaces.h"
#include "types.h"
#include "vm.h"
#include <common/audio/sound/s_soundinternal.h>
#include <palettecontainer.h>
#include <texturemanager.h>
#include <info.h>

namespace DebugServer
{
static const char *basicTypeNames[] = {"NONE", "uint32",	 "int32",			"uint16",				 "int16", "uint8", "int8", "float",			 "double",	"bool",				 "string",
																			 "name", "SpriteID", "TextureID", "TranslationID", "Sound", "Color", "Enum", "StateLabel", "pointer", "VoidPointer", nullptr};

ValueStateNode::ValueStateNode(std::string name, VMValue variable, PType *type, PClass *stateOwningClass)
	: StateNodeNamedVariable(name), m_variable(variable), m_type(type), m_StateOwningClass(stateOwningClass)
{
}

dap::Variable ValueStateNode::ToVariable(const VMValue &m_variable, PType *m_type, PClass *stateOwningClass)
{
	dap::Variable variable;

	auto basic_type = GetBasicType(m_type);
	variable.type = basicTypeNames[basic_type];

	if (m_type == TypeString)
	{
		variable.type = "string";
		if (IsVMValueValid(&m_variable))
		{
			const FString &str = m_variable.s();
			auto chars = isFStringValid(str) ? str.GetChars() : nullptr;
			variable.value = !chars? "\"\"" : StringFormat("\"%s\"", chars);
		}
		else
		{
			variable.value = "<EMPTY>";
		}
	}
	else if (m_type->isPointer())
	{
		if (m_type->isClassPointer())
		{
			variable.type = "ClassPointer";
			auto type = PType::toClassPointer(m_type)->PointedType;
			variable.value = type->DescriptiveName();
		}
		else if (m_type->isFunctionPointer())
		{
			variable.type = PType::toFunctionPointer(m_type)->mDescriptiveName.GetChars();
			if (IsVMValueValid(&m_variable))
			{

				PFunction *func = (PFunction *)m_variable.a;
				if (func->OwningClass)
				{
					variable.value += StringFormat("%s.%s (%p)", func->OwningClass->TypeName.GetChars(), func->SymbolName.GetChars(), m_variable.a);
				}
				else if (func->Variants.size() > 0 && func->Variants[0].Implementation)
				{
					variable.value += StringFormat("%s (%p)", func->Variants[0].Implementation->PrintableName, m_variable.a);
				}
			}
			else
			{
				variable.value = "<NULL>";
			}
		}
		else
		{
			auto pointedType = m_type->toPointer()->PointedType;
			variable.type = std::string("Pointer<") + pointedType->DescriptiveName() + ">";
			if (!IsVMValueValid(&m_variable))
			{
				variable.value = "<NULL>";
			}
			else if (pointedType->isScalar() && !pointedType->isPointer())
			{
				// TODO: TypeState?
				auto val = DerefValue(&m_variable, GetBasicType(pointedType));
				auto deref_var = ToVariable(val, pointedType, stateOwningClass);
				variable.value = StringFormat("%p {%s}", (m_variable.a), deref_var.value.c_str());
			}
			else
			{
				// just display the address
				variable.value = StringFormat("%p", (m_variable.a));
			}
		}
	}
	else if (m_type->isInt())
	{ // explicitly not TYPE_IntNotInt
		int64_t val = TruncateVMValue(&m_variable, basic_type).i;
		if (basic_type == BASIC_uint32 || basic_type == BASIC_uint16 || basic_type == BASIC_uint8){
			variable.value = StringFormat("%u", val);
		} else {
			variable.value = StringFormat("%d", val);
		}
	}
	else if (m_type->isFloat())
	{
		variable.value = StringFormat("%f", TruncateVMValue(&m_variable, basic_type).f);
	}
	else if (m_type->Flags & TT::TypeFlags::TYPE_IntNotInt)
	{
		// PBool
		// PName
		// PSpriteID
		// PTextureID
		// PTranslationID
		// PSound
		// PColor
		// PStateLabel
		// PEnum
		if (m_type == TypeBool)
		{
			variable.type = "bool";
			variable.value = m_variable.i ? "true" : "false";
		}
		else if (m_type == TypeName)
		{
			variable.type = "Name";
			auto name = FName((ENamedName)(m_variable.i));
			variable.value = StringFormat("Name# %d: \'%s\'", m_variable.i, name.GetChars());
		}
		else if (m_type == TypeSpriteID)
		{
			variable.type = "SpriteID";
			// TODO: Get the sprite name? how do they get the sprite name into the serializer?
			variable.value = StringFormat("SpriteID# %d", m_variable.i);
		}
		else if (m_type == TypeTextureID)
		{
			variable.type = "TextureID";
			int val = m_variable.i;
			int *val_ptr = &val;
			FTextureID textureID = *reinterpret_cast<FTextureID *>(val_ptr);
			FGameTexture *gameTexture = TexMan.GetGameTexture(textureID);
			const char *tex_name = gameTexture ? gameTexture->GetName().GetChars() : "<INVALID>";
			variable.value = StringFormat("TextureID# %d (%s)", m_variable.i, tex_name);
		}
		else if (m_type == TypeTranslationID)
		{
			variable.type = "TranslationID";
			FTranslationID translationID = FTranslationID::fromInt(m_variable.i);
			variable.value = StringFormat("TranslationID# %d {type: %d, index: %d}", m_variable.i, GetTranslationType(translationID), GetTranslationIndex(translationID));
		}
		else if (m_type == TypeSound)
		{
			variable.type = "Sound";
			const char *soundName = (m_variable.i > 0 && (uint32_t)m_variable.i < soundEngine->GetNumSounds()) ? soundEngine->GetSoundName(FSoundID::fromInt(m_variable.i)) : "<INVALID>";
			variable.value = StringFormat("Sound# %d (%s)", m_variable.i, soundName);
		}
		else if (m_type == TypeColor)
		{
			variable.type = "Color";
			int soundval = m_variable.i;

			uint8_t a = (soundval >> 24) & 0xFF;
			uint8_t r = (soundval >> 16) & 0xFF;
			uint8_t g = (soundval >> 8) & 0xFF;
			uint8_t b = soundval & 0xFF;
			// hex format
			variable.value = StringFormat("Color #%08X (a: %d, r: %d, g: %d, b: %d)", soundval, a, r, g, b);
		}
		else if (m_type == TypeStateLabel)
		{
			variable.type = "StateLabel";
			std::string label_name = "<unknown>";
			const char *label = nullptr;
			auto NameVal = m_variable.i;
			FName StateName = NAME_None;
			PClassActor *actualOwner = nullptr;
			if (NameVal > 0)
			{
				if (NameVal > 0x10000000)
				{
					NameVal -= 0x10000000;
				}
				auto name = ENamedName(NameVal);
				StateName = FName(name);
			}
			auto state = GetStateFromIdx(m_variable.i, stateOwningClass, actualOwner);
			if (state)
			{
				if (actualOwner && NameVal < 0 && actualOwner->OwnsState(state))
				{
					int stateidx = state - actualOwner->GetStates();
					if (stateidx < actualOwner->GetStateLabels()->NumLabels)
					{
						StateName = actualOwner->GetStateLabels()->Labels[stateidx].Label;
					}
				}
			}
			if (StateName != NAME_None)
			{
				const char *ownerName = actualOwner ? actualOwner->TypeName.GetChars() : "<unknownOwner>";
				label_name = StringFormat("%s.%s", ownerName, StateName.GetChars());
			}
			else
			{
				label_name = "<unknown_state>";
			}
			variable.value = StringFormat("StateLabel# %d: \'%s\'", m_variable.i, label_name.c_str());
		}
		else
		{
			variable.type = m_type->DescriptiveName();
			// check if it begins with "Enum"
			if (ToLowerCopy(variable.type.value("")).find("enum") == 0)
			{
				auto enum_type = static_cast<PEnum *>(m_type);
				// TODO: How to get the enum value names?
				variable.value = StringFormat("%d", m_variable.i);
			}
			else
			{
				variable.value = StringFormat("%d", m_variable.i);
			}
			variable.type = "Enum";
			variable.value = StringFormat("%d", m_variable.i);
		}
	}
	else
	{
		variable.type = m_type->DescriptiveName();
		variable.value = "<ERROR?>";
	}
	return variable;
}

bool ValueStateNode::SerializeToProtocol(dap::Variable &variable)
{
	variable = ToVariable(m_variable, m_type, m_StateOwningClass);
	SetVariableName(variable);
	return true;
}
}
