
#include "ObjectStateNode.h"
#include "common/scripting/dap/GameInterfaces.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>
#include <common/objects/dobject.h>
#include <memory>

namespace DebugServer
{

ObjectStateNode::ObjectStateNode(const std::string &name, VMValue value, PType *asClass, const bool subView) : m_name(name), m_subView(subView), m_value(value), m_class(asClass) { }

bool ObjectStateNode::SerializeToProtocol(dap::Variable &variable)
{
	variable.variablesReference = IsVMValueValid(&m_value) ? GetId() : 0;

	variable.name = m_name;
	variable.type = m_class->mDescriptiveName.GetChars();

	std::vector<std::string> childNames;
	GetChildNames(childNames);

	variable.namedVariables = childNames.size();
	auto typeval = variable.type.value("");
	// check if name begins with 'Pointer<'; if so, remove it, and the trailing '>'
	if (typeval.size() > 9 && typeval.find("Pointer<") == 0 && typeval[typeval.size() - 1] == '>')
	{
		typeval = typeval.substr(8, typeval.size() - 9);
	}
	if (!m_subView)
	{
		if (!m_value.a)
		{
			variable.value = StringFormat("%s <NULL>", typeval.c_str());
		}
		else
		{
			variable.value = StringFormat("%s (%p)", typeval.c_str(), m_value.a);
		}
	}
	else
	{
		variable.value = typeval;
	}

	return true;
}

bool ObjectStateNode::GetChildNames(std::vector<std::string> &names)
{
	if (!m_cachedNames.empty())
	{
		names = m_cachedNames;
		return true;
	}
	auto p_type = m_class;
	if (p_type->isObjectPointer())
	{
		p_type = p_type->toPointer()->PointedType;
	}
	if (p_type->isClass())
	{
		// do the parent class first
		DObject *dobject = IsVMValValidDObject(&m_value) ? static_cast<DObject *>(m_value.a) : nullptr;
		auto classType = PType::toClass(p_type);
		auto descriptor = classType->Descriptor;

		if (classType->ParentType && descriptor && descriptor->ParentClass)
		{
			auto parent = classType->ParentType;
			auto parentName = parent->mDescriptiveName.GetChars();
			m_children[parentName] = RuntimeState::CreateNodeForVariable(parentName, m_value, parent);
			names.push_back(parentName);
		}
		try
		{
			for (auto field : descriptor->Fields)
			{
				auto name = field->SymbolName.GetChars();
				if (!dobject)
				{
					m_children[name] = RuntimeState::CreateNodeForVariable(name, VMValue(), field->Type);
				}
				else
				{
					auto child_val_ptr = GetVMValueVar(dobject, field->SymbolName, field->Type);
					m_children[name] = RuntimeState::CreateNodeForVariable(name, child_val_ptr, field->Type);
				}
				names.push_back(name);
			}
		}
		catch (CRecoverableError &e)
		{

			LogError("Failed to get child names for object '%s' of type %s", m_name.c_str(), p_type->mDescriptiveName.GetChars());
			LogError("Error: %s", e.what());
			return false;
		}
		m_cachedNames = names;
		return true;
	}
	LogError("Failed to get child names for object '%s' of type %s", m_name.c_str(), p_type->mDescriptiveName.GetChars());
	return false;
}

bool ObjectStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
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
	LogError("Failed to get child node '%s' for object '%s'", name.c_str(), m_name.c_str());
	return false;
}

void ObjectStateNode::Reset() { m_children.clear(); }
}
