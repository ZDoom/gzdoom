#include "ArrayStateNode.h"
#include "common/scripting/dap/GameInterfaces.h"
#include "dobject.h"
#include "vm.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>

namespace DebugServer
{
static PType * GetElementType(const PType * p_type)
{
	auto type = p_type;
	if (p_type->isPointer())
	{
		type = dynamic_cast<const PPointer *>(type)->PointedType;
	}
	if (type->isArray())
	{
		auto *arraytype = dynamic_cast<const PArray *>(type);
		return arraytype->ElementType;
	}
	else if (type->isDynArray())
	{
		auto arrayType = dynamic_cast<const PDynArray *>(type);
		return arrayType->ElementType;
	}
	return nullptr;
}

static int64_t GetElementCount(const VMValue &value, PType *p_type)
{
	auto type = p_type;
	auto array_head = value.a;
	if (type->toPointer())
	{
		type = type->toPointer()->PointedType;
	}
	
	if (type->isDynArray() || type->isStaticArray())
	{
		if (!IsVMValueValid(&value))
		{
			return 0;
		}
		
		// FArray has the same layout as TArray, just return count
		auto *arr = static_cast<FArray *>(array_head);
		if (arr->Count == UINT_MAX)
		{
			return -1;
		}
		return arr->Count;
	}
	if (type->isArray())
	{
		if (static_cast<PArray *>(type)->ElementCount == UINT_MAX)
		{
			return -1;
		}
		return static_cast<PArray *>(type)->ElementCount;
	}
	else 
	return 0;
}

ArrayStateNode::ArrayStateNode(std::string name, VMValue value, PType *p_type) : StateNodeNamedVariable(name), m_value(value), m_type(p_type)
{
	auto type = m_type;
	if (type->toPointer())
	{
		type = type->toPointer()->PointedType;
	}
	if (type->isArray())
	{
		auto *arraytype = dynamic_cast<PArray *>(type);
		m_elementType = arraytype->ElementType;
	}
	else if (type->isDynArray())
	{
		auto arrayType = dynamic_cast<PDynArray *>(type);
		m_elementType = arrayType->ElementType;
	}
}

bool ArrayStateNode::SerializeToProtocol(dap::Variable &variable)
{
	variable.variablesReference = GetId();
	auto count = GetElementCount(m_value, m_type);
	variable.indexedVariables = count < 0 ? 0 : count;
	SetVariableName(variable);
	std::string elementTypeName = m_elementType->DescriptiveName();
	variable.type = m_type->mDescriptiveName.GetChars();
	if (!IsVMValueValid(&m_value) || count < 0)
	{
		variable.value = StringFormat("%s[<NONE>]", elementTypeName.c_str());
	}
	else
	{
		variable.value = StringFormat("%s[%d]", elementTypeName.c_str(), variable.indexedVariables.value(0));
		if (m_type->toPointer())
		{
			variable.value += StringFormat(" (%p)", m_value.a);
		}
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
	if (count <= 0)
	{
		return false;
	}

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
	if (m_value.a == nullptr)
	{
		return false;
	}
	auto array_head = m_value.a;
	auto type = m_type;
	if (m_type->isPointer())
	{
		type = static_cast<PPointer *>(m_type)->PointedType;
		// array_head = *static_cast<void **>(m_value.a);
	}
	
	if (type->isDynArray() || type->isStaticArray())
	{
		auto elementType = GetElementType(m_type);
		auto returnType = elementType;
		// too large, return a pointer to the array element
		if (elementType->Size > 8)
		{
			returnType = NewPointer(elementType);
		}
		VMValue element_val;
		if (elementType->isFloat())
		{
			switch (elementType->Size)
			{
			case 8:
				element_val = VMValue(static_cast<TArray<double> *>(array_head)->operator[](elementIndex));
				break;
			case 4:
				element_val = VMValue(static_cast<TArray<float> *>(array_head)->operator[](elementIndex));
				break;
			}
		}
		else if (elementType->isObjectPointer())
		{
			element_val = VMValue(static_cast<TArray<DObject *> *>(array_head)->operator[](elementIndex));
		}
		else if (elementType == TypeString)
		{
			element_val = VMValue(&static_cast<TArray<FString> *>(array_head)->operator[](elementIndex));
		}
		else if (elementType->isPointer())
		{
			element_val = VMValue(static_cast<TArray<void *> *>(array_head)->operator[](elementIndex));
		}
		else
		{
			switch (elementType->Size)
			{
			case 8:
				element_val = VMValue((void *)static_cast<TArray<uint64_t> *>(array_head)->operator[](elementIndex));
				break;
			case 4:
				element_val = VMValue(static_cast<TArray<uint32_t> *>(array_head)->operator[](elementIndex));
				break;
			case 2:
				element_val = VMValue(static_cast<TArray<uint16_t> *>(array_head)->operator[](elementIndex));
				break;
			case 1:
				element_val = VMValue(static_cast<TArray<uint8_t> *>(array_head)->operator[](elementIndex));
				break;
			default:
				// too large, return a ptr to the array element
				assert(unsigned(elementIndex) <= static_cast<FArray *>(array_head)->Count);
				element_val = VMValue((void *)(((char *)static_cast<FArray *>(array_head)->Array) + (elementIndex * elementType->Size)));
			}
		}
		m_children[elidx_str] = RuntimeState::CreateNodeForVariable(elidx_str, element_val, returnType);
	}
	else if (type->isArray())
	{
		auto element_size = m_elementType->Size;
		void *var = (void *)((char *)array_head + (element_size * elementIndex));
		VMValue value = GetVMValue(var, m_elementType);
		// m_children[elidx_str] = RuntimeState::CreateNodeForVariable(std::to_string(elementIndex), DerefValue(&element_ptr, GetBasicType(m_elementType)), m_elementType);

		m_children[elidx_str] = RuntimeState::CreateNodeForVariable(std::to_string(elementIndex), value, m_elementType);
	}
	node = m_children[elidx_str];

	return true;
}
}
