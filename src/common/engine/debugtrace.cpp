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
#include <string>
#include <thread>
#include <unordered_map>

#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_time.h"
#include "m_misc.h"
#include "printf.h"
#include "zstring.h"

CVAR(Bool, debugtrace_enable, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, debugtrace_filter, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, debugtrace_minseverity, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, debugtrace_stats, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

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
	DebugTrace::Severity SeverityLevel = DebugTrace::Severity::Info;
	char Channel[ChannelSize] = {};
	char Message[MessageSize] = {};
};

static TraceEntry TraceBuffer[TraceCapacity] = {};
static std::atomic<uint64_t> NextSerial = 1;

// Statistics tracking
static std::unordered_map<std::string, size_t> ChannelCounts;
static std::atomic<uint64_t> TotalCount = {0};
static std::atomic<uint64_t> SeverityCounts[4] = {{0}, {0}, {0}, {0}};

static uint64_t GetThreadStamp()
{
	return static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

static const char* GetSeverityName(DebugTrace::Severity severity)
{
	switch (severity)
	{
		case DebugTrace::Severity::Debug: return "DEBUG";
		case DebugTrace::Severity::Info: return "INFO";
		case DebugTrace::Severity::Warning: return "WARN";
		case DebugTrace::Severity::Error: return "ERROR";
		default: return "????";
	}
}

static bool ShouldChannelBeLogged(const char* channel)
{
	if (!debugtrace_enable)
		return false;

	const char* filter = debugtrace_filter;
	if (filter == nullptr || *filter == '\0')
		return true;

	// Check if channel matches filter (supports multiple channels separated by commas)
	const char* start = filter;
	while (*start != '\0')
	{
		// Find end of current channel in filter
		const char* end = start;
		while (*end != '\0' && *end != ',')
			++end;

		// Extract channel from filter
		FString filterChannel;
		for (const char* p = start; p < end; ++p)
		{
			if (*p != ' ' && *p != '\t')
				filterChannel += *p;
		}

		if (filterChannel.IsEmpty())
		{
			// Empty filter means all channels
			return true;
		}

		if (channel != nullptr && filterChannel.CompareNoCase(channel) == 0)
			return true;

		// Move to next channel in filter
		start = (*end != '\0') ? end + 1 : end;
	}

	return false;
}

static bool ShouldSeverityBeLogged(DebugTrace::Severity severity)
{
	if (!debugtrace_enable)
		return false;

	const int minSeverity = debugtrace_minseverity;
	return static_cast<int>(severity) >= max(minSeverity, 0);
}

static void UpdateStatistics(const char* channel, DebugTrace::Severity severity)
{
	if (debugtrace_stats)
	{
		if (channel != nullptr)
		{
			std::string channelKey(channel);
			auto it = ChannelCounts.find(channelKey);
			if (it != ChannelCounts.end())
				it->second++;
			else
				ChannelCounts[channelKey] = 1;
		}

		const int severityIndex = static_cast<int>(severity);
		if (severityIndex >= 0 && severityIndex < 4)
			SeverityCounts[severityIndex]++;

		TotalCount++;
	}
}

static void StoreEntry(const char* channel, DebugTrace::Severity severity, const char* message)
{
	if (!debugtrace_enable)
		return;
	if (!ShouldChannelBeLogged(channel))
		return;
	if (!ShouldSeverityBeLogged(severity))
		return;

	UpdateStatistics(channel, severity);

	const uint64_t serial = NextSerial.fetch_add(1, std::memory_order_relaxed);
	TraceEntry& entry = TraceBuffer[(serial - 1u) % TraceCapacity];
	entry.Serial.store(0, std::memory_order_relaxed);

	entry.Milliseconds = I_msTime();
	entry.ThreadStamp = GetThreadStamp();
	entry.SeverityLevel = severity;
	std::snprintf(entry.Channel, sizeof(entry.Channel), "%s", channel != nullptr ? channel : "trace");
	std::snprintf(entry.Message, sizeof(entry.Message), "%s", message != nullptr ? message : "");

	entry.Serial.store(serial, std::memory_order_release);
}

static void PrintEntry(uint64_t serial, const char* channelFilter, DebugTrace::Severity minSeverity)
{
	const TraceEntry& entry = TraceBuffer[(serial - 1u) % TraceCapacity];
	const uint64_t published = entry.Serial.load(std::memory_order_acquire);
	if (published != serial)
		return;

	// Apply filters
	if (channelFilter != nullptr && entry.Channel[0] != '\0')
	{
		const char* filterStart = channelFilter;
		bool matches = false;
		while (*filterStart != '\0' && !matches)
		{
			const char* filterEnd = filterStart;
			while (*filterEnd != '\0' && *filterEnd != ',')
				++filterEnd;

			FString filterChannel;
			for (const char* p = filterStart; p < filterEnd; ++p)
			{
				if (*p != ' ' && *p != '\t')
					filterChannel += *p;
			}

			if (filterChannel.IsEmpty() || filterChannel.CompareNoCase(entry.Channel) == 0)
				matches = true;

			filterStart = (*filterEnd != '\0') ? filterEnd + 1 : filterEnd;
		}
		if (!matches)
			return;
	}

	if (static_cast<int>(entry.SeverityLevel) < static_cast<int>(minSeverity))
		return;

	char channel[ChannelSize] = {};
	char message[MessageSize] = {};
	std::memcpy(channel, entry.Channel, sizeof(channel));
	std::memcpy(message, entry.Message, sizeof(message));

	if (entry.Serial.load(std::memory_order_acquire) != published)
		return;

	Printf("%10llu ms [0x%08llX] [%5s] %s: %s\n",
		static_cast<unsigned long long>(entry.Milliseconds),
		static_cast<unsigned long long>(entry.ThreadStamp & 0xffffffffull),
		GetSeverityName(entry.SeverityLevel),
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
		entry.SeverityLevel = Severity::Info;
		entry.Channel[0] = '\0';
		entry.Message[0] = '\0';
	}

	NextSerial.store(1, std::memory_order_release);

	// Don't clear statistics by default - they're useful for aggregation
	// Use ClearStats() separately if needed
}

void Mark(const char* channel, const char* message)
{
	StoreEntry(channel, Severity::Info, message);
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

	StoreEntry(channel, Severity::Info, message);
}

void Debug(const char* channel, const char* message)
{
	StoreEntry(channel, Severity::Debug, message);
}

void Debugf(const char* channel, const char* format, ...)
{
	if (!debugtrace_enable)
		return;

	char message[MessageSize] = {};
	va_list args;
	va_start(args, format);
	myvsnprintf(message, sizeof(message), format != nullptr ? format : "", args);
	va_end(args);

	StoreEntry(channel, Severity::Debug, message);
}

void Info(const char* channel, const char* message)
{
	StoreEntry(channel, Severity::Info, message);
}

void Infof(const char* channel, const char* format, ...)
{
	if (!debugtrace_enable)
		return;

	char message[MessageSize] = {};
	va_list args;
	va_start(args, format);
	myvsnprintf(message, sizeof(message), format != nullptr ? format : "", args);
	va_end(args);

	StoreEntry(channel, Severity::Info, message);
}

void Warning(const char* channel, const char* message)
{
	StoreEntry(channel, Severity::Warning, message);
}

void Warningf(const char* channel, const char* format, ...)
{
	if (!debugtrace_enable)
		return;

	char message[MessageSize] = {};
	va_list args;
	va_start(args, format);
	myvsnprintf(message, sizeof(message), format != nullptr ? format : "", args);
	va_end(args);

	StoreEntry(channel, Severity::Warning, message);
}

void Error(const char* channel, const char* message)
{
	StoreEntry(channel, Severity::Error, message);
}

void Errorf(const char* channel, const char* format, ...)
{
	if (!debugtrace_enable)
		return;

	char message[MessageSize] = {};
	va_list args;
	va_start(args, format);
	myvsnprintf(message, sizeof(message), format != nullptr ? format : "", args);
	va_end(args);

	StoreEntry(channel, Severity::Error, message);
}

void Dump()
{
	Dump(nullptr, Severity::Debug);
}

void Dump(const char* channelFilter)
{
	Dump(channelFilter, Severity::Debug);
}

void Dump(Severity minSeverity)
{
	Dump(nullptr, minSeverity);
}

void Dump(const char* channelFilter, Severity minSeverity)
{
	const uint64_t lastSerial = NextSerial.load(std::memory_order_acquire) - 1u;
	if (lastSerial == 0u)
	{
		Printf("Recent engine trace: <empty>\n");
		return;
	}

	const uint64_t firstSerial = (lastSerial > TraceCapacity) ? (lastSerial - TraceCapacity + 1u) : 1u;

	FString header = "Recent engine trace";
	if (channelFilter != nullptr && *channelFilter != '\0')
		header.AppendFormat(" [channel=%s]", channelFilter);
	if (minSeverity != Severity::Debug)
		header.AppendFormat(" [severity=%s]", GetSeverityName(minSeverity));

	Printf("%s:\n", header.GetChars());
	for (uint64_t serial = firstSerial; serial <= lastSerial; ++serial)
	{
		PrintEntry(serial, channelFilter, minSeverity);
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

		buffer += mysnprintf(buffer, end - buffer, "%s[%10llu ms][0x%08llX][%5s] %s: %s",
			lfstr,
			static_cast<unsigned long long>(entry.Milliseconds),
			static_cast<unsigned long long>(entry.ThreadStamp & 0xffffffffull),
			GetSeverityName(entry.SeverityLevel),
			channel,
			message);
	}
	*buffer = 0;
}

void ClearStats()
{
	ChannelCounts.clear();
	TotalCount = 0;
	for (int i = 0; i < 4; ++i)
		SeverityCounts[i] = 0;
}

void DumpStats()
{
	Printf("Debug Trace Statistics:\n");

	uint64_t totalCount = TotalCount.load();
	if (totalCount == 0)
	{
		Printf("  No traces recorded.\n");
		return;
	}

	Printf("  Total entries: %llu\n", static_cast<unsigned long long>(totalCount));
	Printf("\n  By Severity:\n");
	Printf("    DEBUG:  %llu\n", static_cast<unsigned long long>(SeverityCounts[0].load()));
	Printf("    INFO:   %llu\n", static_cast<unsigned long long>(SeverityCounts[1].load()));
	Printf("    WARN:   %llu\n", static_cast<unsigned long long>(SeverityCounts[2].load()));
	Printf("    ERROR:  %llu\n", static_cast<unsigned long long>(SeverityCounts[3].load()));

	Printf("\n  By Channel:\n");
	for (const auto& pair : ChannelCounts)
	{
		Printf("    %s: %zu\n", pair.first.c_str(), pair.second);
	}
}

size_t GetTotalCount()
{
	return static_cast<size_t>(TotalCount.load());
}

size_t GetChannelCount(const char* channel)
{
	if (channel == nullptr)
		return 0;

	std::string channelKey(channel);
	auto it = ChannelCounts.find(channelKey);
	return (it != ChannelCounts.end()) ? it->second : 0;
}

bool SaveToFile(const char* filename)
{
	if (filename == nullptr)
		return false;

	FILE* file = fopen(filename, "w");
	if (file == nullptr)
	{
		Printf("Failed to open debug trace file: %s\n", filename);
		return false;
	}

	const uint64_t lastSerial = NextSerial.load(std::memory_order_acquire) - 1u;
	if (lastSerial == 0u)
	{
		fprintf(file, "Debug trace: <empty>\n");
		fclose(file);
		Printf("Debug trace saved to: %s\n", filename);
		return true;
	}

	fprintf(file, "HCDE Debug Trace Export\n");
	fprintf(file, "========================\n");
	fprintf(file, "Total entries: %llu\n\n", static_cast<unsigned long long>(lastSerial));

	const uint64_t firstSerial = (lastSerial > TraceCapacity) ? (lastSerial - TraceCapacity + 1u) : 1u;
	for (uint64_t serial = firstSerial; serial <= lastSerial; ++serial)
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

		fprintf(file, "[%10llu ms][0x%08llX][%5s] %s: %s\n",
			static_cast<unsigned long long>(entry.Milliseconds),
			static_cast<unsigned long long>(entry.ThreadStamp & 0xffffffffull),
			GetSeverityName(entry.SeverityLevel),
			channel,
			message);
	}

	fclose(file);
	Printf("Debug trace saved to: %s\n", filename);
	return true;
}

bool IsChannelEnabled(const char* channel)
{
	return ShouldChannelBeLogged(channel);
}

bool IsSeverityEnabled(Severity severity)
{
	return ShouldSeverityBeLogged(severity);
}
}

CCMD(debugtrace)
{
	if (argv.argc() == 1)
		DebugTrace::Dump();
	else if (argv.argc() == 2)
		DebugTrace::Dump(argv[1]);
	else
		Printf("Usage: debugtrace [channel]\n");
}

CCMD(debugcleartrace)
{
	DebugTrace::Clear();
	Printf("Recent engine trace cleared.\n");
}

CCMD(debugtraceseverity)
{
	if (argv.argc() == 1)
	{
		Printf("Current minimum severity: %d\n", int(debugtrace_minseverity));
		Printf("Severity levels: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR\n");
	}
	else if (argv.argc() == 2)
	{
		int level = 0;
		if (C_IsValidInt(argv[1], level))
		{
			level = clamp<int>(level, 0, 3);
			debugtrace_minseverity = level;
			Printf("Minimum severity set to: %d (%s)\n", level, level == 0 ? "DEBUG" : level == 1 ? "INFO" : level == 2 ? "WARN" : "ERROR");
		}
		else
			Printf("Invalid severity level.\n");
	}
	else
		Printf("Usage: debugtraceseverity [0-3]\n");
}

CCMD(debugtracestats)
{
	if (argv.argc() == 2 && stricmp(argv[1], "clear") == 0)
	{
		DebugTrace::ClearStats();
		Printf("Debug trace statistics cleared.\n");
	}
	else
		DebugTrace::DumpStats();
}

CCMD(debugtracesave)
{
	if (argv.argc() != 2)
	{
		Printf("Usage: debugtracesave <filename>\n");
		return;
	}

	if (DebugTrace::SaveToFile(argv[1]))
		Printf("Saved successfully.\n");
}

CCMD(debugtracefilter)
{
	if (argv.argc() == 1)
	{
		Printf("Current channel filter: %s\n", static_cast<const char*>(debugtrace_filter));
		Printf("Empty filter means all channels are logged.\n");
		Printf("Multiple channels can be separated by commas.\n");
		Printf("Examples: net, rendering, input\n");
	}
	else if (argv.argc() == 2)
	{
		if (stricmp(argv[1], "clear") == 0 || stricmp(argv[1], "") == 0)
		{
			debugtrace_filter = "";
			Printf("Channel filter cleared.\n");
		}
		else
		{
			debugtrace_filter = argv[1];
			Printf("Channel filter set to: %s\n", argv[1]);
		}
	}
	else
		Printf("Usage: debugtracefilter [channel_filter|clear]\n");
}
