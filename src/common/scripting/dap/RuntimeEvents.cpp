#define XBYAK_NO_OP_NAMES

#include "RuntimeEvents.h"

#include <cassert>
#include <mutex>
#include <dap/protocol.h>
#include "GameInterfaces.h"

namespace DebugServer
{
namespace RuntimeEvents
{
#define EVENT_WRAPPER_IMPL(NAME, HANDLER_SIGNATURE)                               \
	eventpp::CallbackList<HANDLER_SIGNATURE> g_##NAME##Event;                     \
                                                                                  \
	NAME##EventHandle SubscribeTo##NAME(std::function<HANDLER_SIGNATURE> handler) \
	{                                                                             \
		return g_##NAME##Event.append(handler);                                   \
	}                                                                             \
                                                                                  \
	bool UnsubscribeFrom##NAME(NAME##EventHandle handle)                          \
	{                                                                             \
		if (!handle)                                                              \
			return false;                                                         \
		return g_##NAME##Event.remove(handle);                                    \
	}

	EVENT_WRAPPER_IMPL(InstructionExecution, void(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc))
	EVENT_WRAPPER_IMPL(CreateStack, void(VMFrameStack *))
	EVENT_WRAPPER_IMPL(CleanupStack, void(uint32_t))
	EVENT_WRAPPER_IMPL(Log, void(int level, const char *msg))
	EVENT_WRAPPER_IMPL(BreakpointChanged, void(const dap::Breakpoint &bpoint, const std::string &))
#undef EVENT_WRAPPER_IMPL


	void EmitBreakpointChangedEvent(const dap::Breakpoint &bpoint, const std::string &what)
	{
		if (!g_BreakpointChangedEvent.empty())
		{
			g_BreakpointChangedEvent(bpoint, what);
		}
	}
	void EmitInstructionExecutionEvent(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc)
	{
		if (!g_InstructionExecutionEvent.empty())
		{
			g_InstructionExecutionEvent(stack, ret, numret, pc);
		}
	}
	void EmitLogEvent(int level, const char *msg)
	{
		if (!g_LogEvent.empty())
		{
			g_LogEvent(level, msg);
		}
	}

	// TODO: Are CreateStack and CleanupStack events needed? VM execution is single-threaded and there's only one stack.
	// Maybe an event when the last frame gets popped off, but I'm not sure what would even need that.

}
}
