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
#include <mutex>
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

struct TraceSnapshot
{
	uint64_t Milliseconds = 0;
	uint64_t ThreadStamp = 0;
	DebugTrace::Severity SeverityLevel = DebugTrace::Severity::Info;
	char Channel[ChannelSize] = {};
	char Message[MessageSize] = {};
};

static TraceEntry TraceBuffer[TraceCapacity] = {};
static std::atomic<uint64_t> NextSerial = 1;
static std::mutex TraceMutex;

// Statistics tracking
static std::unordered_map<std::string, size_t> ChannelCounts;
static std::mutex StatsMutex;
static std::atomic<uint64_t> TotalCount = {0};
static std::atomic<uint64_t> SeverityCounts[4] = {{0}, {0}, {0}, {0}};

static uint64_t GetThreadStamp()
{
	return static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

static const char* SeverityName(DebugTrace::Severity severity)
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

static bool IsAllChannelFilter(const char* filter)
{
	return filter == nullptr || *filter == '\0' || stricmp(filter, "all") == 0 || strcmp(filter, "*") == 0;
}

static bool ChannelMatchesFilter(const char* channel, const char* channelFilter)
{
	if (IsAllChannelFilter(channelFilter))
		return true;

	// Supports multiple channels separated by commas, matching debugtrace_filter.
	const char* start = channelFilter;
	bool hadFilter = false;
	while (*start != '\0')
	{
		const char* end = start;
		while (*end != '\0' && *end != ',')
			++end;

		FString filterChannel;
		for (const char* p = start; p < end; ++p)
		{
			if (*p != ' ' && *p != '\t')
				filterChannel += *p;
		}

		if (filterChannel.IsEmpty())
		{
			start = (*end != '\0') ? end + 1 : end;
			continue;
		}

		hadFilter = true;

		if (channel != nullptr && filterChannel.CompareNoCase(channel) == 0)
			return true;

		start = (*end != '\0') ? end + 1 : end;
	}

	return !hadFilter;
}

static bool ShouldChannelBeLogged(const char* channel)
{
	if (!debugtrace_enable)
		return false;

	const char* filter = debugtrace_filter;
	return ChannelMatchesFilter(channel, filter);
}

static bool ShouldSeverityBeLogged(DebugTrace::Severity severity)
{
	if (!debugtrace_enable)
		return false;

	const int minSeverity = clamp<int>(debugtrace_minseverity, 0, 3);
	return static_cast<int>(severity) >= minSeverity;
}

static void UpdateStatistics(const char* channel, DebugTrace::Severity severity)
{
	if (debugtrace_stats)
	{
		std::lock_guard<std::mutex> lock(StatsMutex);
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
			SeverityCounts[severityIndex].fetch_add(1, std::memory_order_relaxed);

		TotalCount.fetch_add(1, std::memory_order_relaxed);
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

	std::lock_guard<std::mutex> lock(TraceMutex);
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

static bool ReadEntrySnapshot(uint64_t serial, const char* channelFilter, DebugTrace::Severity minSeverity, TraceSnapshot& snapshot)
{
	std::lock_guard<std::mutex> lock(TraceMutex);
	const TraceEntry& entry = TraceBuffer[(serial - 1u) % TraceCapacity];
	const uint64_t published = entry.Serial.load(std::memory_order_acquire);
	if (published != serial)
		return false;

	if (!ChannelMatchesFilter(entry.Channel, channelFilter))
		return false;

	if (static_cast<int>(entry.SeverityLevel) < static_cast<int>(minSeverity))
		return false;

	snapshot.Milliseconds = entry.Milliseconds;
	snapshot.ThreadStamp = entry.ThreadStamp;
	snapshot.SeverityLevel = entry.SeverityLevel;
	std::memcpy(snapshot.Channel, entry.Channel, sizeof(snapshot.Channel));
	std::memcpy(snapshot.Message, entry.Message, sizeof(snapshot.Message));
	return true;
}

static void PrintEntry(uint64_t serial, const char* channelFilter, DebugTrace::Severity minSeverity)
{
	TraceSnapshot snapshot;
	if (!ReadEntrySnapshot(serial, channelFilter, minSeverity, snapshot))
		return;
	Printf("%10llu ms [0x%08llX] [%5s] %s: %s\n",
		static_cast<unsigned long long>(snapshot.Milliseconds),
		static_cast<unsigned long long>(snapshot.ThreadStamp & 0xffffffffull),
		SeverityName(snapshot.SeverityLevel),
		snapshot.Channel,
		snapshot.Message);
}

static void AppendTraceText(char*& cursor, char* end, const char* format, ...)
{
	if (cursor == nullptr || end == nullptr)
		return;

	if (cursor >= end)
	{
		*end = '\0';
		cursor = end;
		return;
	}

	const size_t remaining = static_cast<size_t>(end - cursor);
	va_list args;
	va_start(args, format);
	const int result = myvsnprintf(cursor, remaining, format != nullptr ? format : "", args);
	va_end(args);

	if (result < 0)
	{
		*cursor = '\0';
		return;
	}

	const size_t written = static_cast<size_t>(result);
	if (written >= remaining)
	{
		cursor = end;
		*cursor = '\0';
		return;
	}

	cursor += written;
}
}

namespace DebugTrace
{
const char* GetSeverityName(Severity severity)
{
	return SeverityName(severity);
}

bool ParseSeverity(const char* text, Severity& severity)
{
	if (text == nullptr || *text == '\0')
		return false;

	int level = 0;
	if (C_IsValidInt(text, level))
	{
		severity = static_cast<Severity>(clamp<int>(level, 0, 3));
		return true;
	}

	if (stricmp(text, "debug") == 0)
	{
		severity = Severity::Debug;
		return true;
	}
	if (stricmp(text, "info") == 0)
	{
		severity = Severity::Info;
		return true;
	}
	if (stricmp(text, "warn") == 0 || stricmp(text, "warning") == 0)
	{
		severity = Severity::Warning;
		return true;
	}
	if (stricmp(text, "error") == 0)
	{
		severity = Severity::Error;
		return true;
	}

	return false;
}

void Clear()
{
	std::lock_guard<std::mutex> lock(TraceMutex);
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
		header.AppendFormat(" [severity=%s]", SeverityName(minSeverity));

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
		AppendTraceText(buffer, end, "%sRecent engine trace: <empty>", lfstr != nullptr ? lfstr : "\n");
		*buffer = 0;
		return;
	}

	const uint64_t firstSerial = (lastSerial > TraceCapacity) ? (lastSerial - TraceCapacity + 1u) : 1u;
	AppendTraceText(buffer, end, "%sRecent engine trace:", lfstr != nullptr ? lfstr : "\n");
	for (uint64_t serial = firstSerial; serial <= lastSerial && buffer < end; ++serial)
	{
		TraceSnapshot snapshot;
		if (!ReadEntrySnapshot(serial, nullptr, Severity::Debug, snapshot))
			continue;

		AppendTraceText(buffer, end, "%s[%10llu ms][0x%08llX][%5s] %s: %s",
			lfstr != nullptr ? lfstr : "\n",
			static_cast<unsigned long long>(snapshot.Milliseconds),
			static_cast<unsigned long long>(snapshot.ThreadStamp & 0xffffffffull),
			SeverityName(snapshot.SeverityLevel),
			snapshot.Channel,
			snapshot.Message);
	}
	*buffer = 0;
}

void ClearStats()
{
	std::lock_guard<std::mutex> lock(StatsMutex);
	ChannelCounts.clear();
	TotalCount.store(0, std::memory_order_relaxed);
	for (int i = 0; i < 4; ++i)
		SeverityCounts[i].store(0, std::memory_order_relaxed);
}

void DumpStats()
{
	Printf("Debug Trace Statistics:\n");

	std::lock_guard<std::mutex> lock(StatsMutex);
	uint64_t totalCount = TotalCount.load(std::memory_order_relaxed);
	if (totalCount == 0)
	{
		Printf("  No traces recorded.\n");
		return;
	}

	Printf("  Total entries: %llu\n", static_cast<unsigned long long>(totalCount));
	Printf("\n  By Severity:\n");
	Printf("    DEBUG:  %llu\n", static_cast<unsigned long long>(SeverityCounts[0].load(std::memory_order_relaxed)));
	Printf("    INFO:   %llu\n", static_cast<unsigned long long>(SeverityCounts[1].load(std::memory_order_relaxed)));
	Printf("    WARN:   %llu\n", static_cast<unsigned long long>(SeverityCounts[2].load(std::memory_order_relaxed)));
	Printf("    ERROR:  %llu\n", static_cast<unsigned long long>(SeverityCounts[3].load(std::memory_order_relaxed)));

	Printf("\n  By Channel:\n");
	for (const auto& pair : ChannelCounts)
	{
		Printf("    %s: %zu\n", pair.first.c_str(), pair.second);
	}
}

size_t GetTotalCount()
{
	return static_cast<size_t>(TotalCount.load(std::memory_order_relaxed));
}

size_t GetChannelCount(const char* channel)
{
	if (channel == nullptr)
		return 0;

	std::lock_guard<std::mutex> lock(StatsMutex);
	std::string channelKey(channel);
	auto it = ChannelCounts.find(channelKey);
	return (it != ChannelCounts.end()) ? it->second : 0;
}

bool SaveToFile(const char* filename)
{
	return SaveToFile(filename, nullptr, Severity::Debug);
}

bool SaveToFile(const char* filename, const char* channelFilter, Severity minSeverity)
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
	fprintf(file, "Total entries: %llu\n", static_cast<unsigned long long>(lastSerial));
	if (!IsAllChannelFilter(channelFilter))
		fprintf(file, "Channel filter: %s\n", channelFilter);
	if (minSeverity != Severity::Debug)
		fprintf(file, "Minimum severity: %s\n", SeverityName(minSeverity));
	fprintf(file, "\n");

	const uint64_t firstSerial = (lastSerial > TraceCapacity) ? (lastSerial - TraceCapacity + 1u) : 1u;
	for (uint64_t serial = firstSerial; serial <= lastSerial; ++serial)
	{
		TraceSnapshot snapshot;
		if (!ReadEntrySnapshot(serial, channelFilter, minSeverity, snapshot))
			continue;

		fprintf(file, "[%10llu ms][0x%08llX][%5s] %s: %s\n",
			static_cast<unsigned long long>(snapshot.Milliseconds),
			static_cast<unsigned long long>(snapshot.ThreadStamp & 0xffffffffull),
			SeverityName(snapshot.SeverityLevel),
			snapshot.Channel,
			snapshot.Message);
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

static bool ParseTraceFilterArgs(int argc, FCommandLine& argv, int firstArg, const char*& channelFilter, DebugTrace::Severity& minSeverity)
{
	channelFilter = nullptr;
	minSeverity = DebugTrace::Severity::Debug;

	if (argc <= firstArg)
		return true;

	if (DebugTrace::ParseSeverity(argv[firstArg], minSeverity))
		return argc == firstArg + 1;

	if (!IsAllChannelFilter(argv[firstArg]))
		channelFilter = argv[firstArg];

	if (argc > firstArg + 1)
	{
		if (!DebugTrace::ParseSeverity(argv[firstArg + 1], minSeverity))
			return false;
	}

	return argc <= firstArg + 2;
}

CCMD(debugtrace)
{
	const char* channelFilter = nullptr;
	DebugTrace::Severity minSeverity = DebugTrace::Severity::Debug;
	if (!ParseTraceFilterArgs(argv.argc(), argv, 1, channelFilter, minSeverity))
	{
		Printf("Usage: debugtrace [channel|all] [debug|info|warn|error|0-3]\n");
		return;
	}

	DebugTrace::Dump(channelFilter, minSeverity);
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
		const int level = clamp<int>(int(debugtrace_minseverity), 0, 3);
		Printf("Current minimum severity: %d (%s)\n", level, DebugTrace::GetSeverityName(static_cast<DebugTrace::Severity>(level)));
		Printf("Severity levels: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR\n");
	}
	else if (argv.argc() == 2)
	{
		DebugTrace::Severity severity;
		if (DebugTrace::ParseSeverity(argv[1], severity))
		{
			const int level = static_cast<int>(severity);
			debugtrace_minseverity = level;
			Printf("Minimum severity set to: %d (%s)\n", level, DebugTrace::GetSeverityName(severity));
		}
		else
			Printf("Invalid severity level.\n");
	}
	else
		Printf("Usage: debugtraceseverity [debug|info|warn|error|0-3]\n");
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
	if (argv.argc() < 2)
	{
		Printf("Usage: debugtracesave <filename> [channel|all] [debug|info|warn|error|0-3]\n");
		return;
	}

	const char* channelFilter = nullptr;
	DebugTrace::Severity minSeverity = DebugTrace::Severity::Debug;
	if (!ParseTraceFilterArgs(argv.argc(), argv, 2, channelFilter, minSeverity))
	{
		Printf("Usage: debugtracesave <filename> [channel|all] [debug|info|warn|error|0-3]\n");
		return;
	}

	if (DebugTrace::SaveToFile(argv[1], channelFilter, minSeverity))
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
