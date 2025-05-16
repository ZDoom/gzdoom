#include "BreakpointManager.h"
#include <cstdint>
#include <regex>
#include "Utilities.h"
#include "RuntimeEvents.h"
#include "GameInterfaces.h"

namespace DebugServer
{

int64_t GetBreakpointID(int scriptReference, int lineNumber, uint16_t instruction_number) { return (((int64_t)scriptReference) << 32) + (instruction_number << 16) + lineNumber; }

int AddInvalidBreakpoint(std::vector<dap::Breakpoint> &breakpoints, int line, int sourceRef, int &bpointCount, const std::string &reason, const dap::optional<dap::Source> &source = {})
{
	auto breakpointId = GetBreakpointID(sourceRef, line, bpointCount++);
	breakpoints.push_back(dap::Breakpoint {.id = breakpointId, .line = line, .message = reason, .source = source, .verified = false});
	return breakpointId;
}

dap::ResponseOrError<dap::SetBreakpointsResponse> BreakpointManager::SetBreakpoints(const dap::Source &source, const dap::SetBreakpointsRequest &request)
{
	RETURN_COND_DAP_ERROR(!request.breakpoints.has_value(), "SetBreakpoints: No breakpoints provided!");
	auto &srcBreakpoints = request.breakpoints.value();
	dap::SetBreakpointsResponse response;
	std::set<int> breakpointLines;
	int bpointCount = 0;
	auto scriptPath = source.name.value("");
	auto sourceRef = GetSourceReference(source);

	std::map<int, BreakpointInfo> foundBreakpoints;
	auto addInvalidBreakpoint = [&](int line, const std::string &reason, bool shouldLog = true)
	{
		if (shouldLog)
		{
			LogError("SetBreakpoints: %s", reason.c_str());
		}
		AddInvalidBreakpoint(response.breakpoints, line, sourceRef, bpointCount, reason, source);
	};

	auto binary = m_pexCache->GetScript(source);
	if (!binary)
	{
		for (const auto &srcBreakpoint : srcBreakpoints)
		{
			addInvalidBreakpoint(srcBreakpoint.line, StringFormat("Could not find script %s in loaded sources!", scriptPath.c_str()));
		}
		return response;
	}
	if (binary->functions.empty())
	{
		for (const auto &srcBreakpoint : srcBreakpoints)
		{
			addInvalidBreakpoint(srcBreakpoint.line, StringFormat("Script %s is present but not loaded", scriptPath.c_str()));
		}
		return response;
	}
	else if (binary->functionLineMap.empty())
	{
		for (const auto &srcBreakpoint : srcBreakpoints)
		{
			addInvalidBreakpoint(srcBreakpoint.line, StringFormat("No debug info found for script %s", scriptPath.c_str()));
		}
	}
	ClearLineBreakpoints(sourceRef);

	for (const auto &srcBreakpoint : srcBreakpoints)
	{
		int breakpointsSet = 0;
		int line = static_cast<int>(srcBreakpoint.line);
		int instructionNum = -1;
		int foundFunctionInfoIndex;
		int64_t breakpointId = -1;
		auto found = binary->functionLineMap.find_ranges(line);
		if (found.size() == 0)
		{
			addInvalidBreakpoint(line, "Invalid instruction", false);
			continue;
		}

		while (!found.empty())
		{
			auto func = found.top()->mapped();
			if (func == nullptr || IsFunctionAbstract(func) || func->LineInfoCount == 0)
			{
				found.pop();
				continue;
			}
			for (int i = 0; i < func->LineInfoCount; i++)
			{
				if (func->LineInfo[i].LineNumber == line)
				{
					instructionNum = func->LineInfo[i].InstructionIndex;
					foundFunctionInfoIndex = i;
					break;
				}
			}
			if (instructionNum == -1)
			{
				found.pop();
				continue;
			}


			breakpointId = GetBreakpointID(sourceRef, instructionNum, bpointCount++);
			void *instrRef = func->Code + instructionNum;

			auto bpoint = BreakpointInfo {
				.type = BreakpointInfo::Type::Line,
				.ref = sourceRef,
				.breakpointId = breakpointId,
				.instructionNum = instructionNum,
				.lineNum = line,
				.debugFuncInfoIndex = foundFunctionInfoIndex,
				.address = instrRef,
				.isNative = false};
			m_breakpoints[instrRef] = bpoint;
			// Only send back one breakpoint per line, or the debugger will get confused
			if (breakpointsSet == 0)
			{
				response.breakpoints.push_back(
					{.id = dap::integer(breakpointId),
					 .instructionReference = StringFormat("%p", instrRef),
					 .line = dap::integer(line),
					 //.offset = foundLine ? dap::integer(instructionNum) : dap::optional<dap::integer>(),
					 .source = source,
					 .verified = true});
			}
			found.pop();
			breakpointsSet++;
		}
		if (breakpointId == -1)
		{
			addInvalidBreakpoint(line, StringFormat("No function found for line %d in script %s", line, scriptPath.c_str()));
		}
	}

	return response;
}

dap::ResponseOrError<dap::SetFunctionBreakpointsResponse> BreakpointManager::SetFunctionBreakpoints(const dap::SetFunctionBreakpointsRequest &request)
{
	auto &breakpoints = request.breakpoints;
	dap::SetFunctionBreakpointsResponse response;
	// each request clears the previous function breakpoints
	m_nativeFunctionBreakpoints.clear();
	int bpointCount = 0;

	for (const auto &breakpoint : breakpoints)
	{
		auto fullFuncName = breakpoint.name;
		// function names are `class.function`
		auto func_name_parts = Split(fullFuncName, ".");

		if (func_name_parts.size() != 2)
		{
			AddInvalidBreakpoint(response.breakpoints, 1, -1, bpointCount, StringFormat("Invalid function name %s", fullFuncName.c_str()));
			continue;
		}
		auto className = FName(func_name_parts[0]);
		auto functionName = FName(func_name_parts[1]);
		auto func = PClass::FindFunction(className, functionName);
		if (!func)
		{
			AddInvalidBreakpoint(response.breakpoints, 1, -1, bpointCount, StringFormat("Could not find function %s in loaded sources!", fullFuncName.c_str()));
			continue;
		}
		if (IsFunctionNative(func))
		{
			auto bpoint_info = BreakpointInfo {.breakpointId = GetBreakpointID(GetScriptReference(func->QualifiedName), 1, 0), .instructionNum = 0, .lineNum = 1, .debugFuncInfoIndex = 0, .isNative = true};
			m_nativeFunctionBreakpoints[func->QualifiedName] = bpoint_info;
			response.breakpoints.push_back(dap::Breakpoint {
				.id = bpoint_info.breakpointId,
				.line = 1,
			});
			continue;
		}
		// script function
		auto scriptFunction = static_cast<VMScriptFunction *>(func);
		auto scriptName = scriptFunction->SourceFileName.GetChars();
		dap::Source source;
		if (!m_pexCache->GetSourceData(scriptName, source))
		{
			AddInvalidBreakpoint(response.breakpoints, 1, -1, bpointCount, StringFormat("Could not find script %s in loaded sources!", scriptName));
			continue;
		}

		auto ref = GetSourceReference(source);
		if (scriptFunction->LineInfoCount == 0)
		{
			AddInvalidBreakpoint(response.breakpoints, 1, ref, bpointCount, StringFormat("Could not find line info for function %s!", fullFuncName.c_str()));
			continue;
		}
		auto lineNum = scriptFunction->LineInfo[0].LineNumber;
		auto instructionNum = scriptFunction->LineInfo[0].InstructionIndex;
		auto breakpointId = GetBreakpointID(ref, instructionNum, bpointCount++);
		void *instrRef = scriptFunction->Code + instructionNum;

		auto bpoint_info = BreakpointInfo {.breakpointId = breakpointId, .instructionNum = instructionNum, .lineNum = lineNum, .debugFuncInfoIndex = 0, .address = instrRef, .isNative = false};
		response.breakpoints.push_back(dap::Breakpoint {
			.id = breakpointId,
			.instructionReference = StringFormat("%p", scriptFunction->Code),
			.line = lineNum,
			.source = source,
			.verified = true,
		});
		m_breakpoints[(void *)scriptFunction->Code] = bpoint_info;
	}
	return response;
}

void BreakpointManager::ClearBreakpoints(bool emitChanged)
{
	if (emitChanged)
	{
		for (auto &kv : m_breakpoints)
		{
			InvalidateAllBreakpointsForScript(kv.second.ref, emitChanged);
		}
	}
	m_breakpoints.clear();
}

void BreakpointManager::ClearLineBreakpoints(int ref)
{
	std::vector<void *> toRemove;
	for (auto &KV : m_breakpoints)
	{
		auto bpinfo = KV.second;
		if (bpinfo.ref != ref || bpinfo.type != BreakpointInfo::Type::Line)
		{
			continue;
		}
		toRemove.push_back(KV.first);
	}
	for (auto &key : toRemove)
	{
		m_breakpoints.erase(key);
	}
}

void BreakpointManager::InvalidateAllBreakpointsForScript(int ref, bool emitChanged)
{
	std::vector<void *> toRemove;
	dap::Source source;
	if (emitChanged)
	{
		auto binary = m_pexCache->GetCachedScript(ref);
		if (!binary)
		{
			LogError("Could not find source for script reference %d", ref);
			source.sourceReference = ref;
		}
		else
		{
			source = binary->GetDapSource();
		}
	}
	for (auto &KV : m_breakpoints)
	{
		auto bpinfo = KV.second;
		if (bpinfo.ref != ref)
		{
			continue;
		}
		toRemove.push_back(KV.first);
		if (emitChanged)
		{
			RuntimeEvents::EmitBreakpointChangedEvent(dap::Breakpoint {.id = bpinfo.breakpointId, .line = bpinfo.lineNum, .source = source, .verified = false}, "changed");
		}
	}
	for (auto &key : toRemove)
	{
		m_breakpoints.erase(key);
	}
}


bool BreakpointManager::GetExecutionIsAtValidBreakpoint(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc)
{
#define CLEAR_AND_RETURN   m_last_seen = nullptr; return false
	if (!IsFunctionNative(stack->TopFrame()->Func) && m_breakpoints.find((void *)pc) != m_breakpoints.end())
	{
		return true;
	}
	else if (IsFunctionNative(stack->TopFrame()->Func) && m_nativeFunctionBreakpoints.find(stack->TopFrame()->Func->QualifiedName) != m_nativeFunctionBreakpoints.end())
	{
		return true;
	}
	return CLEAR_AND_RETURN;
#undef CLEAR_AND_RETURN
}


bool BreakpointManager::HasSeenBreakpoint(BreakpointManager::BreakpointInfo *info)
{
	if (!m_last_seen || m_last_seen != info)
	{
		return false;
	}
	return true;
}

dap::ResponseOrError<dap::SetInstructionBreakpointsResponse> BreakpointManager::SetInstructionBreakpoints(const dap::SetInstructionBreakpointsRequest &request)
{
	RETURN_DAP_ERROR("Instruction breakpoints are not supported yet!");
}

}
