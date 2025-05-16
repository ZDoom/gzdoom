
#include "StructStateNode.h"
#include "types.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>
#include <common/objects/dobject.h>
#include <common/scripting/core/symbols.h>

namespace DebugServer
{ // std::string name, VMValue* value, PType* knownType
StructStateNode::StructStateNode(std::string name, VMValue value, PType *knownType) : m_name(name), m_value(value), m_type(knownType) { }

bool StructStateNode::SerializeToProtocol(dap::Variable &variable)
{
	variable.variablesReference = IsVMValueValid(&m_value) ? GetId() : 0;
	variable.namedVariables = 0;
	std::vector<std::string> names;
	GetChildNames(names);
	variable.namedVariables = names.size();
	variable.name = m_name;
	variable.type = m_type->DescriptiveName();
	auto typeval = variable.type.value("");
	// check if name begins with 'Pointer<'; if so, remove it, and the trailing '>'
	if (typeval.size() > 9 && typeval.find("Pointer<") == 0 && typeval[typeval.size() - 1] == '>')
	{
		typeval = typeval.substr(8, typeval.size() - 9);
	}
	if (!m_value.a)
	{
		variable.value = StringFormat("%s <NULL>", typeval.c_str());
	}
	else
	{
		variable.value = StringFormat("%s (%p)", typeval.c_str(), m_value.a);
	}
	return true;
}

bool StructStateNode::GetChildNames(std::vector<std::string> &names)
{
	if (m_children.size() > 0)
	{
		for (auto &pair : m_children)
		{
			names.push_back(pair.first);
		}
		return true;
	}
	auto structType = static_cast<PContainerType *>(m_type->isPointer() ? static_cast<PPointer *>(m_type)->PointedType : m_type);
	auto it = structType->Symbols.GetIterator();
	PSymbolTable::MapType::Pair *pair;
	bool not_done;
	char *struct_ptr = static_cast<char *>(m_value.a);
	size_t struct_size = structType->Size;
	size_t currsize = 0;
	while (it.NextPair(pair))
	{
		auto name = pair->Key.GetChars();
		if (!isValidDobject(pair->Value))
		{
			// invalid field, we won't show it
			continue;
		}
		try
		{
			auto field = dynamic_cast<PField *>(pair->Value);
			if (field)
			{
				auto flags = field->Flags;
				auto fieldSize = field->Type->Size;
				names.push_back(name);
				auto type = field->Type;
				if (!struct_ptr)
				{
					m_children[name] = RuntimeState::CreateNodeForVariable(name, VMValue(), field->Type);
				}
				else
				{
					VMValue val;
					if (type->isScalar())
					{
						switch (fieldSize)
						{
						case 1:
							val = VMValue(*(uint8_t *)struct_ptr);
							break;
						case 2:
							val = VMValue(*(uint16_t *)struct_ptr);
							break;
						case 4:
							val = VMValue(*(uint32_t *)struct_ptr);
							break;
						case 8:
							val = VMValue((void *)(*(uint64_t *)struct_ptr));
							break;
						default:
							LogError("StructStateNode::GetChildNames: scalar field size not supported");
							break;
						}
					}
					else
					{
						val = VMValue((void *)struct_ptr);
					}
					m_children[name] = RuntimeState::CreateNodeForVariable(name, val, field->Type);
				}
				// increment struct_ptr by fieldSize
				// don't increment if it's a transient field
				if (struct_ptr && !(flags & VARF_Transient))
				{
					currsize += fieldSize;
					struct_ptr = struct_ptr + fieldSize;
				}
			}
		}
		catch (...)
		{
		}
		if (currsize > struct_size)
		{
			LogError("StructStateNode::GetChildNames: struct size exceeded, breaking");
			break;
		}
	}
	return true;
}

bool StructStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (m_children.empty())
	{
		std::vector<std::string> names;
		GetChildNames(names);
	}
	if (m_children.find(name) != m_children.end())
	{
		node = m_children[name];
		return true;
	}
	return true;
}
}
