
#include "ObjectStateNode.h"
#include "common/scripting/dap/GameInterfaces.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>
#include <common/objects/dobject.h>
#include <memory>

namespace DebugServer
{

ObjectStateNode::ObjectStateNode(const std::string &name, VMValue value, PType *asClass, const bool subView)
	: StateNodeNamedVariable(name), m_subView(subView), m_value(value), m_ClassType(asClass)
{
}

bool ObjectStateNode::SerializeToProtocol(dap::Variable &variable)
{
	variable.variablesReference = IsVMValValidDObject(&m_value) ? GetId() : 0;
	auto pointedType = m_ClassType->isObjectPointer() ? m_ClassType->toPointer()->PointedType : m_ClassType;
	SetVariableName(variable);
	const char *typeName = pointedType->mDescriptiveName.GetChars();
	variable.type = typeName;
	std::vector<std::string> childNames;
	GetChildNames(childNames);
	variable.namedVariables = childNames.size();
	if (m_ClassType->isObjectPointer())
	{
		if (!m_value.a)
		{
			variable.value = StringFormat("%s <NULL>", typeName);
		}
		else if (m_VMType != nullptr && !m_subView && pointedType != m_VMType)
		{
			// If this is something that isn't actually descended from the class...
			if (!PType::toClass(m_VMType)->Descriptor->IsDescendantOf(PType::toClass(pointedType)->Descriptor))
			{
				variable.value = StringFormat("%s (%p) as %s", m_VMType->mDescriptiveName.GetChars(), m_value.a, typeName);
			}
			else
			{
				variable.value = StringFormat("%s (%p)", m_VMType->mDescriptiveName.GetChars(), m_value.a);
			}
		}
		else
		{
			variable.value = StringFormat("%s (%p)", typeName, m_value.a);
		}
	}
	else if (!m_subView)
	{
		variable.value = typeName;
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
	auto p_type = m_ClassType->isObjectPointer() ? m_ClassType->toPointer()->PointedType : m_ClassType;
	if (p_type->isClass())
	{
		// do the parent class first
		DObject *dobject = IsVMValValidDObject(&m_value) ? static_cast<DObject *>(m_value.a) : nullptr;
		if (!dobject)
		{
			return false;
		}
		m_VMType = dobject->GetClass()->VMType;
		auto classType = PType::toClass(p_type);
		auto descriptor = classType->Descriptor;
		// If the VMType is something else (and this isn't a view into the parent class properties)...
		if (!m_subView && m_VMType && m_VMType != classType && m_VMType->isClass())
		{
			classType = PType::toClass(m_VMType);
			descriptor = dobject->GetClass();
		}
		std::string error_msg;
		m_cachedNames.reserve(descriptor->Fields.Size() + 1);
		if (classType->ParentType && descriptor && descriptor->ParentClass)
		{
			auto parent = classType->ParentType;
			auto parentName = parent->mDescriptiveName.GetChars();
			m_children[parentName] = std::make_shared<ObjectStateNode>(parentName, m_value, parent, true);
			m_virtualChildren[parentName] = m_children[parentName];
			m_cachedNames.push_back(parentName);
		}
		try
		{
			for (auto field : descriptor->Fields)
			{
				auto name = field->SymbolName.GetChars();
				if (!dobject)
				{
					m_children[name] = RuntimeState::CreateNodeForVariable(name, VMValue(), field->Type, nullptr, descriptor);
				}
				else
				{
					try
					{
						auto child_val_ptr = GetVMValueVar(dobject, field->SymbolName, field->Type, field->BitValue);
						m_children[name] = RuntimeState::CreateNodeForVariable(name, child_val_ptr, field->Type, nullptr, descriptor);
					}
					catch (CRecoverableError &e)
					{
						error_msg = e.what();
						// class is not actually its descriptor (this is the case where things are intentionally set to destroyed objects, like `PendingWeapon = WP_NOCHANGE`)
						// try again with the actual class
						m_children.clear();
						m_cachedNames.clear();
						descriptor = dobject->GetClass();
						for (auto field : descriptor->Fields)
						{
							name = field->SymbolName.GetChars();
							auto child_val_ptr = GetVMValueVar(dobject, field->SymbolName, field->Type, field->BitValue);
							m_children[name] = RuntimeState::CreateNodeForVariable(name, child_val_ptr, field->Type, nullptr, descriptor);
							m_cachedNames.emplace_back(name);
						}
						break;
					}
				}
				m_cachedNames.emplace_back(name);
			}
		}
		catch (CRecoverableError &e)
		{

			LogError("Failed to get child names for object '%s' of type %s", m_name.c_str(), p_type->mDescriptiveName.GetChars());
			if (!error_msg.empty())
			{
				LogError("Error: %s", error_msg.c_str());
			}
			LogError("Error: %s", e.what());

			return false;
		}
		names = m_cachedNames;
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
	return false;
}

void ObjectStateNode::Reset() { m_children.clear(); }

caseless_path_map<std::shared_ptr<StateNodeBase>> ObjectStateNode::GetVirtualContainerChildren()
{
	caseless_path_map<std::shared_ptr<StateNodeBase>> children;
	for (auto &child : m_virtualChildren)
	{
		children[child.first] = child.second;
	}
	return children;
}
}
