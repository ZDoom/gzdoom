#pragma once
#include <functional>
#include <eventpp/callbacklist.h>
#include "vm.h"

#define EVENT_DECLARATION(NAME, HANDLER_SIGNATURE)                                 \
    typedef eventpp::CallbackList<HANDLER_SIGNATURE>::Handle NAME##EventHandle;    \
    NAME##EventHandle SubscribeTo##NAME(std::function<HANDLER_SIGNATURE> handler); \
    bool UnsubscribeFrom##NAME(NAME##EventHandle handle);


class VMScriptFunction;
class VMFrameStack;
union VMOP;
class VMReturn;

namespace dap
{
struct Breakpoint;
}

namespace DebugServer
{
namespace RuntimeEvents
{
	EVENT_DECLARATION(InstructionExecution, void(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc))
	EVENT_DECLARATION(CreateStack, void(VMFrameStack *))
	EVENT_DECLARATION(CleanupStack, void(uint32_t))
	EVENT_DECLARATION(Log, void(int level, const char *message))
	EVENT_DECLARATION(BreakpointChanged, void(const dap::Breakpoint &bpoint, const std::string &))
	EVENT_DECLARATION(
		ExceptionThrown, void(VMScriptFunction *sfunc, VMOP *line, EVMAbortException reason, const std::string &message, const std::string &stackTrace))

	void EmitBreakpointChangedEvent(const dap::Breakpoint &bpoint, const std::string &what);
	void EmitInstructionExecutionEvent(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc);
	void EmitLogEvent(int level, const char *message);
	void EmitExceptionEvent(VMScriptFunction *sfunc, VMOP *line, EVMAbortException reason, const std::string &message, const std::string &stackTrace);
	namespace Internal
	{
		void CommitHooks();
	}

}
}

#undef EVENT_DECLARATION