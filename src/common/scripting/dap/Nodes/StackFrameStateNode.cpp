#include "StackFrameStateNode.h"


#include <common/scripting/dap/Utilities.h>
#include <string>

#include "LocalScopeStateNode.h"
#include "RegistersScopeStateNode.h"
#include "GlobalScopeStateNode.h"

namespace DebugServer
{
StackFrameStateNode::StackFrameStateNode(VMFunction *nativeFunction, VMFrame *parentStackFrame)
{
	m_fakeStackFrame.Func = nativeFunction;
	m_fakeStackFrame.ParentFrame = parentStackFrame;
	m_fakeStackFrame.PC = nullptr;
	m_fakeStackFrame.NumRegD = 0;
	m_fakeStackFrame.NumRegF = 0;
	m_fakeStackFrame.NumRegS = 0;
	m_fakeStackFrame.NumRegA = 0;
	m_fakeStackFrame.MaxParam = 0;
	m_fakeStackFrame.NumParam = 0;
	m_stackFrame = &m_fakeStackFrame;
	m_globalsScope = std::make_shared<GlobalScopeStateNode>();
}

StackFrameStateNode::StackFrameStateNode(VMFrame *stackFrame) : m_stackFrame(stackFrame), m_fakeStackFrame()
{
	if (!IsFunctionNative(m_stackFrame->Func))
	{
		auto scriptFunction = dynamic_cast<VMScriptFunction *>(m_stackFrame->Func);
		if (scriptFunction)
		{
			m_localScope = std::make_shared<LocalScopeStateNode>(m_stackFrame);
		}
		m_registersScope = std::make_shared<RegistersScopeStateNode>(m_stackFrame);
	}
	m_globalsScope = std::make_shared<GlobalScopeStateNode>();
}

bool StackFrameStateNode::SerializeToProtocol(dap::StackFrame &stackFrame, PexCache *pexCache) const
{
	stackFrame.id = GetId();
	dap::Source source;
	if (IsFunctionNative(m_stackFrame->Func))
	{
		stackFrame.name = m_stackFrame->Func->PrintableName;
		stackFrame.source = {};
		source.name = "<NATIVE>";
		source.origin = "<NATIVE>";
		source.presentationHint = "deemphasize";
		return true;
	}
	auto scriptFunction = static_cast<VMScriptFunction *>(m_stackFrame->Func);
	std::string ScriptName = scriptFunction->SourceFileName.GetChars();
	if (pexCache->GetSourceData(ScriptName.c_str(), source))
	{
		stackFrame.source = source;
		if (m_stackFrame->PC)
		{
			int lineNumber = scriptFunction->PCToLine(m_stackFrame->PC);
			if (lineNumber > 0)
			{
				stackFrame.line = lineNumber;
				stackFrame.column = 1;
			}
			else if (lineNumber == -1 && scriptFunction->LineInfoCount > 0)
			{
				// end of the function, get the max line number
				int max_line = 0;
				for (int i = 0; i < scriptFunction->LineInfoCount; i++)
				{
					if (scriptFunction->LineInfo[i].LineNumber > max_line)
					{
						max_line = scriptFunction->LineInfo[i].LineNumber;
					}
				}
				stackFrame.line = max_line + 1;
				stackFrame.column = 1;
			}
			stackFrame.instructionPointerReference = StringFormat("%p", m_stackFrame->PC);
		}
	}

	stackFrame.name = m_stackFrame->Func->PrintableName;

	return true;
}

bool StackFrameStateNode::GetChildNames(std::vector<std::string> &names)
{
	auto scriptFunction = GetVMScriptFunction(m_stackFrame->Func);
	if (scriptFunction)
	{
		names.push_back("Local");
	}
	names.push_back("Globals");
	if (scriptFunction)
	{
		names.push_back("Registers");
	}
	return true;
}

bool StackFrameStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (IsFunctionNative(m_stackFrame->Func))
	{
		return false;
	}
	if (CaseInsensitiveEquals(name, "Globals"))
	{
		node = m_globalsScope;
		return true;
	}
	if (CaseInsensitiveEquals(name, "Registers"))
	{
		node = m_registersScope;
		return true;
	}
	if (CaseInsensitiveEquals(name, "local") && !IsFunctionNative(m_stackFrame->Func))
	{
		auto scriptFunction = dynamic_cast<VMScriptFunction *>(m_stackFrame->Func);
		if (scriptFunction)
		{
			node = m_localScope;
			return true;
		}
	}
	return false;
}
}
