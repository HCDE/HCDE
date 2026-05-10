/*
** debugtrace.cpp
**
** Engine-side debug trace capture helpers.
**
**-----------------------------------------------------------------------------
**
** Copyright 2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**-----------------------------------------------------------------------------
**
*/

#include "debugtrace.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <thread>

#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_time.h"
#include "printf.h"

CVAR(Bool, debugtrace_enable, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

namespace
{
constexpr size_t TraceCapacity = 256u;
constexpr size_t ChannelSize = 32u;
constexpr size_t MessageSize = 224u;

struct TraceEntry
{
	std::atomic<uint64_t> Serial = 0;
	uint64_t Milliseconds = 0;
	uint64_t ThreadStamp = 0;
	char Channel[ChannelSize] = {};
	char Message[MessageSize] = {};
};

static TraceEntry TraceBuffer[TraceCapacity] = {};
static std::atomic<uint64_t> NextSerial = 1;

static uint64_t GetThreadStamp()
{
	return static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

static void StoreEntry(const char* channel, const char* message)
{
	if (!debugtrace_enable)
		return;

	const uint64_t serial = NextSerial.fetch_add(1, std::memory_order_relaxed);
	TraceEntry& entry = TraceBuffer[(serial - 1u) % TraceCapacity];
	entry.Serial.store(0, std::memory_order_relaxed);

	entry.Milliseconds = I_msTime();
	entry.ThreadStamp = GetThreadStamp();
	std::snprintf(entry.Channel, sizeof(entry.Channel), "%s", channel != nullptr ? channel : "trace");
	std::snprintf(entry.Message, sizeof(entry.Message), "%s", message != nullptr ? message : "");

	entry.Serial.store(serial, std::memory_order_release);
}

static void PrintEntry(uint64_t serial)
{
	const TraceEntry& entry = TraceBuffer[(serial - 1u) % TraceCapacity];
	const uint64_t published = entry.Serial.load(std::memory_order_acquire);
	if (published != serial)
		return;

	char channel[ChannelSize] = {};
	char message[MessageSize] = {};
	std::memcpy(channel, entry.Channel, sizeof(channel));
	std::memcpy(message, entry.Message, sizeof(message));

	if (entry.Serial.load(std::memory_order_acquire) != published)
		return;

	Printf("%10llu ms [0x%08llX] %s: %s\n",
		static_cast<unsigned long long>(entry.Milliseconds),
		static_cast<unsigned long long>(entry.ThreadStamp & 0xffffffffull),
		channel,
		message);
}
}

namespace DebugTrace
{
void Clear()
{
	for (auto& entry : TraceBuffer)
	{
		entry.Serial.store(0, std::memory_order_release);
		entry.Milliseconds = 0;
		entry.ThreadStamp = 0;
		entry.Channel[0] = '\0';
		entry.Message[0] = '\0';
	}

	NextSerial.store(1, std::memory_order_release);
}

void Mark(const char* channel, const char* message)
{
	StoreEntry(channel, message);
}

void Markf(const char* channel, const char* format, ...)
{
	if (!debugtrace_enable)
		return;

	char message[MessageSize] = {};
	va_list args;
	va_start(args, format);
	myvsnprintf(message, sizeof(message), format != nullptr ? format : "", args);
	va_end(args);

	StoreEntry(channel, message);
}

void Dump()
{
	const uint64_t lastSerial = NextSerial.load(std::memory_order_acquire) - 1u;
	if (lastSerial == 0u)
	{
		Printf("Recent engine trace: <empty>\n");
		return;
	}

	const uint64_t firstSerial = (lastSerial > TraceCapacity) ? (lastSerial - TraceCapacity + 1u) : 1u;
	Printf("Recent engine trace:\n");
	for (uint64_t serial = firstSerial; serial <= lastSerial; ++serial)
	{
		PrintEntry(serial);
	}
}

void WriteCrashInfo(char* buffer, size_t bufflen, const char* lfstr)
{
	if (!debugtrace_enable || buffer == nullptr || bufflen == 0u)
		return;

	char* const end = buffer + bufflen - 1;
	if (buffer >= end)
	{
		*buffer = 0;
		return;
	}

	const uint64_t lastSerial = NextSerial.load(std::memory_order_acquire) - 1u;
	if (lastSerial == 0u)
	{
		buffer += mysnprintf(buffer, end - buffer, "%sRecent engine trace: <empty>", lfstr);
		*buffer = 0;
		return;
	}

	const uint64_t firstSerial = (lastSerial > TraceCapacity) ? (lastSerial - TraceCapacity + 1u) : 1u;
	buffer += mysnprintf(buffer, end - buffer, "%sRecent engine trace:", lfstr);
	for (uint64_t serial = firstSerial; serial <= lastSerial && buffer < end; ++serial)
	{
		const TraceEntry& entry = TraceBuffer[(serial - 1u) % TraceCapacity];
		const uint64_t published = entry.Serial.load(std::memory_order_acquire);
		if (published != serial)
			continue;

		char channel[ChannelSize] = {};
		char message[MessageSize] = {};
		std::memcpy(channel, entry.Channel, sizeof(channel));
		std::memcpy(message, entry.Message, sizeof(message));

		if (entry.Serial.load(std::memory_order_acquire) != published)
			continue;

		buffer += mysnprintf(buffer, end - buffer, "%s[%10llu ms][0x%08llX] %s: %s",
			lfstr,
			static_cast<unsigned long long>(entry.Milliseconds),
			static_cast<unsigned long long>(entry.ThreadStamp & 0xffffffffull),
			channel,
			message);
	}
	*buffer = 0;
}
}

CCMD(debugtrace)
{
	DebugTrace::Dump();
}

CCMD(debugcleartrace)
{
	DebugTrace::Clear();
	Printf("Recent engine trace cleared.\n");
}
