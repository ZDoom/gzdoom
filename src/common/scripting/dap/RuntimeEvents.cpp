#define XBYAK_NO_OP_NAMES

#include "RuntimeEvents.h"

#include <cassert>
#include <dap/protocol.h>
#include "GameInterfaces.h"
#include "common/scripting/vm/vmintern.h"

namespace DebugServer
{
namespace RuntimeEvents
{
#define EVENT_WRAPPER_IMPL(NAME, HANDLER_SIGNATURE)                               \
	bool g_## NAME## EventActive = false;																    \
	std::function<HANDLER_SIGNATURE> g_## NAME## Event;                     \
                                                                                  \
	NAME##EventHandle SubscribeTo##NAME(std::function<HANDLER_SIGNATURE> handler) \
	{                                                                             \
		g_## NAME## Event = handler;\
		g_## NAME## EventActive = true;                                               \
		return handler;\
	}\
	\
	bool UnsubscribeFrom## NAME(NAME## EventHandle handle)\
	{\
		if (!handle)																															\
			return false;                                                         \
		g_## NAME## EventActive = false;                                               \
		g_## NAME## Event  = nullptr;\
		return true;\
	}

	EVENT_WRAPPER_IMPL(InstructionExecution, void(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc))
	EVENT_WRAPPER_IMPL(CreateStack, void(VMFrameStack *))
	EVENT_WRAPPER_IMPL(CleanupStack, void(uint32_t))
	EVENT_WRAPPER_IMPL(Log, void(int level, const char *msg))
	EVENT_WRAPPER_IMPL(BreakpointChanged, void(const dap::Breakpoint &bpoint, const std::string &))
	EVENT_WRAPPER_IMPL(ExceptionThrown, void(EVMAbortException reason, const std::string &message, const std::string &stackTrace))
	EVENT_WRAPPER_IMPL(DebuggerEnabled, bool(void))

#undef EVENT_WRAPPER_IMPL


	void EmitBreakpointChangedEvent(const dap::Breakpoint &bpoint, const std::string &what)
	{
		if (g_BreakpointChangedEventActive)
		{
			g_BreakpointChangedEvent(bpoint, what);
		}
	}
	void EmitInstructionExecutionEvent(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc)
	{
		if (g_InstructionExecutionEventActive)
		{
			g_InstructionExecutionEvent(stack, ret, numret, pc);
		}
	}
	void EmitLogEvent(int level, const char *msg)
	{
		if (g_LogEventActive)
		{
			g_LogEvent(level, msg);
		}
	}
	void EmitExceptionEvent(EVMAbortException reason, const std::string &message, const std::string &stackTrace)
	{
		if (g_ExceptionThrownEventActive)
		{
			g_ExceptionThrownEvent(reason, message, stackTrace);
		}
	}

	bool IsDebugServerRunning()
	{
		if (g_DebuggerEnabledEventActive){
			return g_DebuggerEnabledEvent();
		}
		return false;
	}

	// TODO: Are CreateStack and CleanupStack events needed? VM execution is single-threaded and there's only one stack.
	// Maybe an event when the last frame gets popped off, but I'm not sure what would even need that.

}
}
