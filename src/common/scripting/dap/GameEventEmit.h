#pragma once
#include "vm.h"

class VMFrameStack;

namespace DebugServer
{
namespace RuntimeEvents
{
	void EmitInstructionExecutionEvent(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc);
	void EmitLogEvent(int level, const char *message);
	void EmitExceptionEvent(EVMAbortException reason, const std::string &message, const std::string &stackTrace);
	bool IsDebugServerRunning();
}
}