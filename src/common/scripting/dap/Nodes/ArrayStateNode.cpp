#include "ArrayStateNode.h"
#include "common/scripting/dap/GameInterfaces.h"
#include "dobject.h"
#include "vm.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>

namespace DebugServer
{
constexpr size_t GetElementCount(const VMValue &value, PType *p_type)
{
	if (p_type->isArray())
	{
		return static_cast<PArray *>(p_type)->ElementCount;
	}
	else if (p_type->isDynArray())
	{
		if (!IsVMValueValid(&value))
		{
			return 0;
		}

		auto arrayType = static_cast<PDynArray *>(p_type);
		auto elementType = arrayType->ElementType;
		if (elementType->isFloat())
		{
			switch (elementType->Size)
			{
			case 8:
				return static_cast<TArray<double> *>(value.a)->size();
			case 4:
				return static_cast<TArray<float> *>(value.a)->size();
			}
		}
		else if (elementType->isPointer())
		{
			return static_cast<TArray<void *> *>(value.a)->size();
		}
		else
		{
			switch (elementType->Size)
			{
			case 8:
				return static_cast<TArray<uint64_t> *>(value.a)->size();
			case 4:
				return static_cast<TArray<uint32_t> *>(value.a)->size();
			case 2:
				return static_cast<TArray<uint16_t> *>(value.a)->size();
			case 1:
				return static_cast<TArray<uint8_t> *>(value.a)->size();
			}
		}
	}
	return 0;
}

ArrayStateNode::ArrayStateNode(std::string name, VMValue value, PType *type) : m_name(name), m_value(value), m_type(type)
{
	if (m_type->isArray())
	{
		PArray *arraytype = static_cast<PArray *>(m_type);
		m_elementType = arraytype->ElementType;
	}
	else if (m_type->isDynArray())
	{
		auto arrayType = static_cast<PDynArray *>(m_type);
		m_elementType = arrayType->ElementType;
	}
}

bool ArrayStateNode::SerializeToProtocol(dap::Variable &variable)
{
	variable.variablesReference = GetId();
	variable.indexedVariables = GetElementCount(m_value, m_type);

	variable.name = m_name;

	std::string elementTypeName = m_elementType->DescriptiveName();

	variable.type = StringFormat("%s[]", elementTypeName.c_str());

	if (!IsVMValueValid(&m_value))
	{
		variable.value = StringFormat("%s[<NONE>]", elementTypeName.c_str());
	}
	else
	{
		variable.value = StringFormat("%s[%d]", elementTypeName.c_str(), variable.indexedVariables.value(0));
	}

	return true;
}

bool ArrayStateNode::GetChildNames(std::vector<std::string> &names)
{
	if (!IsVMValueValid(&m_value))
	{
		return true;
	}
	auto count = GetElementCount(m_value, m_type);
	for (uint32_t i = 0; i < count; i++)
	{
		names.push_back(std::to_string(i));
	}

	return true;
}

bool ArrayStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (!IsVMValueValid(&m_value))
	{
		return false;
	}
	auto count = GetElementCount(m_value, m_type);

	int elementIndex;
	if (!ParseInt(name, &elementIndex))
	{
		return false;
	}
	if (elementIndex < 0 || ((uint32_t)elementIndex) > count - 1)
	{
		return false;
	}
	auto elidx_str = std::to_string(elementIndex);

	if (m_children.find(elidx_str) != m_children.end())
	{
		node = m_children[elidx_str];
		return true;
	}
	if (m_type->isArray())
	{
		auto element_size = m_elementType->Size;
		VMValue element_ptr = VMValue((void *)((char *)m_value.a + (element_size * elementIndex)));
		m_children[elidx_str] = RuntimeState::CreateNodeForVariable(std::to_string(elementIndex), DerefValue(&element_ptr, GetBasicType(m_elementType)), m_elementType);
	}
	else if (m_type->isDynArray())
	{
		auto arrayType = static_cast<PDynArray *>(m_type);
		auto elementType = arrayType->ElementType;
		VMValue element_val = [&]
		{
			if (elementType->isFloat())
			{
				switch (elementType->Size)
				{
				case 8:
					return VMValue(static_cast<TArray<double> *>(m_value.a)->operator[](elementIndex));
				case 4:
					return VMValue(static_cast<TArray<float> *>(m_value.a)->operator[](elementIndex));
				}
			}
			else if (elementType->isObjectPointer())
			{
				return VMValue(static_cast<TArray<DObject *> *>(m_value.a)->operator[](elementIndex));
			}
			else if (elementType == TypeString)
			{
				return VMValue(&static_cast<TArray<FString> *>(m_value.a)->operator[](elementIndex));
			}
			else if (elementType->isPointer())
			{
				return VMValue(static_cast<TArray<void *> *>(m_value.a)->operator[](elementIndex));
			}
			else
			{
				switch (elementType->Size)
				{
				case 4:
					return VMValue(static_cast<TArray<uint32_t> *>(m_value.a)->operator[](elementIndex));
				case 2:
					return VMValue(static_cast<TArray<uint16_t> *>(m_value.a)->operator[](elementIndex));
				case 1:
					return VMValue(static_cast<TArray<uint8_t> *>(m_value.a)->operator[](elementIndex));
				}
			}
			return VMValue();
		}();
		m_children[elidx_str] = RuntimeState::CreateNodeForVariable(elidx_str, element_val, elementType);
	}
	node = m_children[elidx_str];

	return true;
}
}
