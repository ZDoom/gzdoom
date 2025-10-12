#include "StateNodeBase.h"
namespace DebugServer
{
int StateNodeBase::GetId() const
{
	return m_id;
}

void StateNodeBase::SetId(const uint32_t id)
{
	m_id = id;
}

StateNodeNamedVariable::StateNodeNamedVariable(const std::string &name, const std::string &evalName)
	: m_name(name), m_evalName(evalName.empty() ? name : evalName)
{
}
std::string StateNodeNamedVariable::GetName() const
{
	return m_name;
}
std::string StateNodeNamedVariable::GetEvalName() const
{
	return m_evalName;
}
void StateNodeNamedVariable::SetName(const std::string &name)
{
	m_name = name;
}
void StateNodeNamedVariable::SetEvalName(const std::string &evalName)
{
	m_evalName = evalName;
}
void StateNodeNamedVariable::SetVariableName(dap::Variable &variable)
{
	variable.name = m_name;
	if (m_evalName != m_name)
	{
		variable.evaluateName = m_evalName;
	}
}

} // namespace DebugServer
