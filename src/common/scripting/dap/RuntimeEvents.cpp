/*
** RuntimeEvents.cpp
**
**
**
**---------------------------------------------------------------------------
**
** Copyright 2025 nikitalita
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
** Code written prior to 2026 is also licensed under:
**
** SPDX-License-Identifier: MIT
**
**---------------------------------------------------------------------------
**
*/

#define XBYAK_NO_OP_NAMES

#include "RuntimeEvents.h"

#include <atomic>
#include <cassert>
#include <mutex>
#include <dap/protocol.h>
#include "GameInterfaces.h"
#include "debugtrace.h"
#include "common/scripting/vm/vmintern.h"

namespace DebugServer
{
namespace RuntimeEvents
{
#define EVENT_WRAPPER_IMPL(NAME, HANDLER_SIGNATURE)                               \
	static std::atomic_bool g_##NAME##EventActive = false;                        \
	static std::mutex g_##NAME##EventMutex;                                       \
	static std::function<HANDLER_SIGNATURE> g_##NAME##Event;                      \
																				  \
	NAME##EventHandle SubscribeTo##NAME(std::function<HANDLER_SIGNATURE> handler) \
	{                                                                             \
		std::lock_guard<std::mutex> lock(g_##NAME##EventMutex);                   \
		g_##NAME##Event = handler;                                                \
		g_##NAME##EventActive.store(!!g_##NAME##Event, std::memory_order_release); \
		DebugTrace::Debugf("debugger", "subscribed runtime event %s", #NAME);    \
		return handler;                                                          \
	}                                                                            \
	                                                                             \
	bool UnsubscribeFrom##NAME(NAME##EventHandle handle)                          \
	{                                                                            \
		if (!handle)                                                             \
			return false;                                                         \
		std::lock_guard<std::mutex> lock(g_##NAME##EventMutex);                   \
		if (!g_##NAME##EventActive.load(std::memory_order_acquire))               \
			return false;                                                         \
		g_##NAME##EventActive.store(false, std::memory_order_release);            \
		g_##NAME##Event = nullptr;                                                \
		DebugTrace::Debugf("debugger", "unsubscribed runtime event %s", #NAME);  \
		return true;                                                             \
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
		BreakpointChangedEventHandle handler;
		if (!g_BreakpointChangedEventActive.load(std::memory_order_acquire))
			return;
		{
			std::lock_guard<std::mutex> lock(g_BreakpointChangedEventMutex);
			if (!g_BreakpointChangedEventActive.load(std::memory_order_relaxed) || !g_BreakpointChangedEvent)
				return;
			handler = g_BreakpointChangedEvent;
		}
		if (handler)
		{
			handler(bpoint, what);
		}
	}
	void EmitInstructionExecutionEvent(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc)
	{
		InstructionExecutionEventHandle handler;
		if (!g_InstructionExecutionEventActive.load(std::memory_order_acquire))
			return;
		{
			std::lock_guard<std::mutex> lock(g_InstructionExecutionEventMutex);
			if (!g_InstructionExecutionEventActive.load(std::memory_order_relaxed) || !g_InstructionExecutionEvent)
				return;
			handler = g_InstructionExecutionEvent;
		}
		if (handler)
		{
			handler(stack, ret, numret, pc);
		}
	}
	void EmitLogEvent(int level, const char *msg)
	{
		LogEventHandle handler;
		if (!g_LogEventActive.load(std::memory_order_acquire))
			return;
		{
			std::lock_guard<std::mutex> lock(g_LogEventMutex);
			if (!g_LogEventActive.load(std::memory_order_relaxed) || !g_LogEvent)
				return;
			handler = g_LogEvent;
		}
		if (handler)
		{
			handler(level, msg);
		}
	}
	void EmitExceptionEvent(EVMAbortException reason, const std::string &message, const std::string &stackTrace)
	{
		ExceptionThrownEventHandle handler;
		if (!g_ExceptionThrownEventActive.load(std::memory_order_acquire))
			return;
		{
			std::lock_guard<std::mutex> lock(g_ExceptionThrownEventMutex);
			if (!g_ExceptionThrownEventActive.load(std::memory_order_relaxed) || !g_ExceptionThrownEvent)
				return;
			handler = g_ExceptionThrownEvent;
		}
		if (handler)
		{
			handler(reason, message, stackTrace);
		}
	}

	bool IsDebugServerRunning()
	{
		DebuggerEnabledEventHandle handler;
		if (!g_DebuggerEnabledEventActive.load(std::memory_order_acquire))
			return false;
		{
			std::lock_guard<std::mutex> lock(g_DebuggerEnabledEventMutex);
			if (!g_DebuggerEnabledEventActive.load(std::memory_order_relaxed) || !g_DebuggerEnabledEvent)
				return false;
			handler = g_DebuggerEnabledEvent;
		}
		return handler ? handler() : false;
	}

	// TODO: Are CreateStack and CleanupStack events needed? VM execution is single-threaded and there's only one stack.
	// Maybe an event when the last frame gets popped off, but I'm not sure what would even need that.

}
}
