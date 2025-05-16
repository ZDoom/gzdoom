#pragma once
#include "vm.h"

class VMFrameStack;

namespace dap
{
struct Breakpoint;
}

namespace DebugServer
{
namespace RuntimeEvents
{
	void EmitInstructionExecutionEvent(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc);
	void EmitLogEvent(int level, const char *message);
	void EmitExceptionEvent(VMScriptFunction *sfunc, VMOP *line, EVMAbortException reason, const std::string &message, const std::string &stackTrace);
}
}