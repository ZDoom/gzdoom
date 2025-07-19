#include "BreakpointManager.h"
#include <cstdint>
#include <regex>
#include "Utilities.h"
#include "RuntimeEvents.h"
#include "GameInterfaces.h"
#include "DebugExecutionManager.h"

namespace DebugServer
{

int64_t BreakpointManager::GetBreakpointID()
{
	++m_CurrentID;
	int64_t id = m_CurrentID;
	if (id < 0)
	{
		m_CurrentID = 1;
		id = m_CurrentID;
	}
	return id;
}


int BreakpointManager::AddInvalidBreakpoint(
	std::vector<dap::Breakpoint> &breakpoints, int line, void *address, const std::string &reason, const dap::optional<dap::Source> &source = {})
{
	auto breakpointId = GetBreakpointID();
	dap::Breakpoint &breakpoint = breakpoints.emplace_back();
	breakpoint.id = breakpointId;
	breakpoint.message = reason;
	breakpoint.source = source;
	breakpoint.verified = false;
	breakpoint.reason = "failed";
	if (line)
	{
		breakpoint.line = line;
	}
	if (address)
	{
		breakpoint.instructionReference = AddrToString(nullptr, address);
	}
	return breakpointId;
}

bool BreakpointManager::AddBreakpointInfo(
	const std::shared_ptr<Binary> &binary,
	VMScriptFunction *function,
	int line,
	void *p_instrRef,
	int offset,
	BreakpointInfo::Type type,
	std::vector<dap::Breakpoint> &r_bpoint,
	const std::string &funcText)
{
	// Only call this with positional breakpoints (line, script function, instruction)
	assert(p_instrRef != nullptr);
	int64_t breakpointId = GetBreakpointID();
	int sourceRef = -1;
	if (binary)
	{
		sourceRef = binary->GetScriptRef();
	}
	auto instrRef = (void *)(static_cast<char *>(p_instrRef) + offset);
	bool found = false;
	if (m_breakpoints.find(instrRef) != m_breakpoints.end())
	{
		for (auto &binfo : m_breakpoints[instrRef])
		{
			if (binfo.type == type)
			{
				if (sourceRef == -1 || binfo.ref == sourceRef)
				{
					return false;
				}
			}
		}
	}
	else
	{
		m_breakpoints[instrRef] = {};
	}
	auto &binfo = m_breakpoints[instrRef].emplace_back();
	binfo.type = type;
	binfo.ref = sourceRef;
	binfo.funcBreakpointText = funcText;
	binfo.bpoint.id = breakpointId;
	binfo.bpoint.line = line;
	binfo.bpoint.instructionReference = AddrToString(function, p_instrRef);
	if (offset)
	{
		binfo.bpoint.offset = offset;
	}
	if (binary) binfo.bpoint.source = binary->GetDapSource();
	binfo.bpoint.verified = false;
	// Only send back one breakpoint per line for line breakpoints, or the DAP client will get confused
	if (type == BreakpointInfo::Type::Line)
	{
		for (auto &kv : m_breakpoints)
		{
			for (auto &existing : kv.second)
			{
				// not the one we just added and the same type
				if (binfo.bpoint.id != existing.bpoint.id && existing.type == type)
				{
					if ((sourceRef == -1 || existing.ref == sourceRef) && (existing.bpoint.line.value(0) == line))
					{
						return false;
					}
				}
			}
		}
	}
	binfo.bpoint.verified = true;
	r_bpoint.push_back(binfo.bpoint);

	return true;
}

void BreakpointManager::GetBpointsForResponse(BreakpointInfo::Type type, std::vector<dap::Breakpoint> &responseBpoints)
{
	for (const auto &bPoints : m_breakpoints)
	{
		if (bPoints.second.empty())
		{
			continue;
		}
		if (bPoints.second[0].type != type)
		{
			continue;
		}
		for (const auto &bp : bPoints.second)
		{
			responseBpoints.push_back(bp.bpoint);
		}
	}
}

dap::ResponseOrError<dap::SetBreakpointsResponse> BreakpointManager::SetBreakpoints(const dap::Source &source, const dap::SetBreakpointsRequest &request)
{
	RETURN_COND_DAP_ERROR(!request.breakpoints.has_value(), "SetBreakpoints: No breakpoints provided!");
	auto &srcBreakpoints = request.breakpoints.value();
	dap::SetBreakpointsResponse response;
	std::set<int> breakpointLines;
	auto scriptPath = source.name.value("");
	auto sourceRef = GetSourceReference(source);

	std::map<int, BreakpointInfo> foundBreakpoints;
	auto addInvalidBreakpoint = [&](int line, const std::string &reason, bool shouldLog = true)
	{
		if (shouldLog)
		{
			LogError("SetBreakpoints: %s", reason.c_str());
		}
		AddInvalidBreakpoint(response.breakpoints, line, nullptr, reason, source);
	};
	ClearBreakpointsForScript(sourceRef, BreakpointInfo::Type::Line);

	auto binary = m_pexCache->GetScript(source);
	if (!binary)
	{
		// check if the archive name is loaded
		auto archive_name = source.origin.value("");
		int containerNum = fileSystem.CheckIfResourceFileLoaded(archive_name.c_str());
		std::string error_message = containerNum == -1 ? StringFormat("%s: Archive %s not loaded", scriptPath.c_str(), archive_name.c_str())
																									 : StringFormat("%s: Could not find script in loaded sources!", scriptPath.c_str());
		for (const auto &srcBreakpoint : srcBreakpoints)
		{
			addInvalidBreakpoint(srcBreakpoint.line, error_message);
		}
		return response;
	}
	if (!binary->HasFunctions())
	{
		for (const auto &srcBreakpoint : srcBreakpoints)
		{
			addInvalidBreakpoint(srcBreakpoint.line, StringFormat("Script %s is present but not loaded", scriptPath.c_str()));
		}
		return response;
	}
	else if (!binary->HasFunctionLines())
	{
		for (const auto &srcBreakpoint : srcBreakpoints)
		{
			addInvalidBreakpoint(srcBreakpoint.line, StringFormat("No debug info found for script %s", scriptPath.c_str()));
		}
	}
	int srcRef = binary->GetScriptRef();
	for (const auto &srcBreakpoint : srcBreakpoints)
	{
		int breakpointsSet = 0;
		int line = static_cast<int>(srcBreakpoint.line);
		int instructionNum = -1;
		int64_t breakpointId = -1;
		auto found = binary->FindFunctionRangesByLine(line);
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
			for (unsigned int i = 0; i < func->LineInfoCount; i++)
			{
				if (func->LineInfo[i].LineNumber == line)
				{
					instructionNum = func->LineInfo[i].InstructionIndex;
					break;
				}
			}
			if (instructionNum == -1)
			{
				found.pop();
				continue;
			}


			void *instrRef = func->Code + instructionNum;
			auto actualBin = binary;

			// Mixin; find the actual script
			if (srcRef != GetScriptReference(func->SourceFileName.GetChars()))
			{
				actualBin = m_pexCache->GetScript(func->SourceFileName.GetChars());
				if (!actualBin)
				{
					addInvalidBreakpoint(line, StringFormat("Could not find script %s in loaded sources!", func->SourceFileName.GetChars()));
					found.pop();
					continue;
				}
			}
			if (AddBreakpointInfo(actualBin, func, line, instrRef, 0, BreakpointInfo::Type::Line, response.breakpoints))
			{
				breakpointId = response.breakpoints.back().id.value(-1);
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
	ClearBreakpointsType(BreakpointInfo::Type::Function);
	m_nativeFunctionBreakpoints.clear();
	int bpointCount = 0;

	for (const auto &breakpoint : breakpoints)
	{
		auto fullFuncName = breakpoint.name;
		// function names are `class.function`
		auto func_name_parts = Split(fullFuncName, ".");

		if (func_name_parts.size() != 2)
		{
			AddInvalidBreakpoint(response.breakpoints, 1, nullptr, StringFormat("Invalid function name %s", fullFuncName.c_str()));
			continue;
		}
		auto className = FName(func_name_parts[0]);
		auto functionName = FName(func_name_parts[1]);
		auto cls = PClass::FindClass(className);
		auto func = PClass::FindFunction(className, functionName);
		if (!func)
		{
			AddInvalidBreakpoint(response.breakpoints, 1, nullptr, StringFormat("Could not find function %s in loaded sources!", fullFuncName.c_str()));
			continue;
		}
		if (IsFunctionNative(func))
		{
			BreakpointInfo bpoint_info;
			bpoint_info.type = BreakpointInfo::Type::Function;
			bpoint_info.ref = -1;
			bpoint_info.funcBreakpointText = fullFuncName;
			bpoint_info.bpoint.id = GetBreakpointID();
			bpoint_info.bpoint.line = 1;
			bpoint_info.bpoint.verified = true;
			m_nativeFunctionBreakpoints[func->QualifiedName] = bpoint_info;
			response.breakpoints.push_back(bpoint_info.bpoint);
			continue;
		}
		// script function
		auto scriptFunction = dynamic_cast<VMScriptFunction *>(func);
		auto scriptName = scriptFunction->SourceFileName.GetChars();
		dap::Source source;
		auto binary = m_pexCache->GetScript(scriptName);
		if (!binary)
		{
			AddInvalidBreakpoint(response.breakpoints, 1, nullptr, StringFormat("Could not find script %s in loaded sources!", scriptName));
			continue;
		}
		if (scriptFunction->LineInfoCount == 0)
		{
			AddInvalidBreakpoint(response.breakpoints, 1, nullptr, StringFormat("Could not find line info for function %s!", fullFuncName.c_str()), source);
			continue;
		}
		auto lineNum = scriptFunction->LineInfo[0].LineNumber;
		auto instructionNum = scriptFunction->LineInfo[0].InstructionIndex;
		void *instrRef = scriptFunction->Code + instructionNum;
		AddBreakpointInfo(binary, scriptFunction, lineNum, instrRef, 0, BreakpointInfo::Type::Function, response.breakpoints, fullFuncName);
	}
	return response;
}

void BreakpointManager::ClearBreakpoints(bool emitChanged)
{
	if (emitChanged)
	{
		std::vector<int> refs;
		for (auto &kv : m_breakpoints)
		{
			for (auto bpointInfo : kv.second)
			{
				if (emitChanged && bpointInfo.bpoint.verified)
				{
					bpointInfo.bpoint.verified = false;
					RuntimeEvents::EmitBreakpointChangedEvent(bpointInfo.bpoint, "changed");
				}
			}
		}
	}
	m_breakpoints.clear();
}

void BreakpointManager::ClearBreakpointsType(BreakpointInfo::Type type)
{
	std::vector<void *> toRemove;
	for (auto &KV : m_breakpoints)
	{
		auto bpinfos = KV.second;
		for (int64_t i = bpinfos.size() - 1; i >= 0; i--)
		{
			if (bpinfos[i].type == type)
			{
				bpinfos.erase(bpinfos.begin() + i);
			}
		}
		if (bpinfos.empty())
		{
			toRemove.push_back(KV.first);
		}
	}
	for (auto &key : toRemove)
	{
		m_breakpoints.erase(key);
	}
}

void BreakpointManager::ClearBreakpointsForScript(int ref, BreakpointInfo::Type type, bool emitChanged)
{
	std::vector<void *> toRemove;
	for (auto &KV : m_breakpoints)
	{
		auto bpinfos = KV.second;
		for (int64_t i = bpinfos.size() - 1; i >= 0; i--)
		{
			auto &bpinfo = bpinfos[i];
			if (bpinfo.ref == ref && (type == BreakpointInfo::Type::NONE || type == bpinfo.type))
			{
				if (emitChanged && bpinfo.bpoint.verified)
				{
					bpinfo.bpoint.verified = false;
					RuntimeEvents::EmitBreakpointChangedEvent(bpinfo.bpoint, "changed");
				}
				bpinfos.erase(bpinfos.begin() + i);
			}
			if (bpinfos.empty())
			{
				toRemove.push_back(KV.first);
			}
		}
	}
	for (auto &key : toRemove)
	{
		m_breakpoints.erase(key);
	}
}


bool BreakpointManager::GetExecutionIsAtValidBreakpoint(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc)
{
	return m_breakpoints.find((void *)pc) != m_breakpoints.end() || (!m_nativeFunctionBreakpoints.empty() && IsAtNativeBreakpoint(stack));
}

inline bool BreakpointManager::IsAtNativeBreakpoint(VMFrameStack *stack)
{
	return PCIsAtNativeCall(stack->TopFrame())
		&& m_nativeFunctionBreakpoints.find(GetCalledFunction(stack->TopFrame())->QualifiedName) != m_nativeFunctionBreakpoints.end();
}

void BreakpointManager::SetBPStoppedEventInfo(VMFrameStack *stack, dap::StoppedEvent &event)
{
	std::vector<dap::integer> breakpoints;
	if (!stack->HasFrames())
	{
		return;
	}
	auto frame = stack->TopFrame();
	std::string description = "Paused on breakpoint";
	if (m_breakpoints.find((void *)frame->PC) != m_breakpoints.end())
	{
		for (auto &bpoint : m_breakpoints[(void *)frame->PC])
		{
			breakpoints.push_back(bpoint.bpoint.id.value(-1));
		}
	}
	if (IsAtNativeBreakpoint(stack))
	{
		auto func = GetCalledFunction(frame);
		auto &bpoint_info = m_nativeFunctionBreakpoints[func->QualifiedName];
		description = std::string("Paused on breakpoint at '") + bpoint_info.funcBreakpointText + "'";
		if (!CaseInsensitiveEquals(bpoint_info.funcBreakpointText, func->QualifiedName))
		{
			event.text = description + " (" + func->QualifiedName + ")";
		}
		else
		{
			event.text = description;
		}
		breakpoints.push_back(m_nativeFunctionBreakpoints[func->QualifiedName].bpoint.id.value(-1));
	}
	if (breakpoints.empty())
	{
		LogInternalError("No breakpoints found for stopped event");
	}
	if (!description.empty())
	{
		event.description = description;
	}
	event.reason = "breakpoint";
	event.hitBreakpointIds = breakpoints;
}

dap::ResponseOrError<dap::SetInstructionBreakpointsResponse> BreakpointManager::SetInstructionBreakpoints(const dap::SetInstructionBreakpointsRequest &request)
{
	auto breakpoints = request.breakpoints;

	dap::SetInstructionBreakpointsResponse response;
	ClearBreakpointsType(BreakpointInfo::Type::Instruction);
	for (unsigned int i = 0; i < breakpoints.size(); i++)
	{
		auto &bp = breakpoints[i];
		void *srcAddress = (void *)(std::stoull(bp.instructionReference.substr(2), nullptr, 16));
		int64_t offset = bp.offset.value(0);
		void *address = offset + static_cast<char *>(srcAddress);

		auto found = m_pexCache->GetFunctionsAtAddress(address);
		if (found.empty())
		{
			AddInvalidBreakpoint(response.breakpoints, 1, address, StringFormat("No function found for address %p", address));
			continue;
		}
		else
		{
			auto func = found.front();
			auto bpoint_info = BreakpointInfo {};

			bpoint_info.type = BreakpointInfo::Type::Instruction;
			int ref;
			auto scriptFunc = dynamic_cast<VMScriptFunction *>(func);

			if (IsFunctionNative(func) || !scriptFunc)
			{
				AddInvalidBreakpoint(response.breakpoints, 1, address, StringFormat("Instruction breakpoints are not supported for native functions"));
				continue;
			}
			auto line = scriptFunc->LineInfo[0].LineNumber;
			auto binary = m_pexCache->GetScript(scriptFunc->SourceFileName.GetChars());
			dap::Breakpoint bpoint;
			AddBreakpointInfo(binary, scriptFunc, line, srcAddress, (int)offset, BreakpointInfo::Type::Instruction, response.breakpoints);
		}
	}
	return response;
}

}
