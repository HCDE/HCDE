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
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_time.h"
#include "m_misc.h"
#include "printf.h"
#include "zstring.h"

CVAR(Bool, debugtrace_enable, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, debugtrace_filter, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, debugtrace_minseverity, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, debugtrace_stats, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, debugtrace_capacity, 16384, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, debugtrace_stream, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, debugtrace_stream_rotate_mb, 10, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, debugtrace_stream_rotate_count, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

namespace
{
// CVAR(...) actually declares an FBoolCVarRef / FIntCVarRef / FStringCVarRef whose internal `ref`
// pointer is not bound until the CVar registry runs (in C_InitConsoleBackend, much later than
// some of our trace sites). Dereferencing the Ref before binding crashes (read of [null+0x60]),
// so we route every CVar read in this translation unit through these guards.
inline bool SafeBool(FBoolCVarRef& cvar, bool def) noexcept
{
	return cvar.get() != nullptr ? bool(cvar) : def;
}

inline int SafeInt(FIntCVarRef& cvar, int def) noexcept
{
	return cvar.get() != nullptr ? int(cvar) : def;
}

inline const char* SafeString(FStringCVarRef& cvar, const char* def) noexcept
{
	if (cvar.get() == nullptr)
		return def != nullptr ? def : "";
	const char* value = static_cast<const char*>(cvar);
	return value != nullptr ? value : (def != nullptr ? def : "");
}
constexpr size_t DefaultTraceCapacity = 16384u;
constexpr size_t MinTraceCapacity = 256u;
constexpr size_t MaxTraceCapacity = 65536u;
constexpr size_t ChannelSize = 32u;
constexpr size_t MessageSize = 496u;
constexpr size_t ProcessTagSize = 16u;
constexpr size_t StreamFlushIntervalMS = 250u;
// Throttle for the "latest.log" mirror copy. Copying the full stream file to
// hcde_trace.<proc>.latest.log on every Warning entry was a primary contributor
// to user-visible lag: with even one Warning every ~250ms the playsim thread
// spent its time blocking on a 2-3MB file copy that the OS cache then had to
// re-write. The mirror is meant to be a "convenient pointer to the most recent
// session log" for shipping diagnostics, not a per-event tail. We keep a slow
// timer-driven refresh (so the file isn't stale by more than a few seconds in
// a long session) and still mirror unconditionally on Errors and on explicit
// FlushStream/rotation/shutdown calls.
constexpr uint64_t LatestStreamMirrorIntervalMS = 5000u;

struct TraceEntry
{
	uint64_t Serial = 0;
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

static std::vector<TraceEntry> TraceBuffer;
static std::atomic<uint64_t> NextSerial = 1;
static std::mutex TraceMutex;

static uint32_t SessionId = 0u;
static char ProcessTag[ProcessTagSize] = "hcde";
static bool SessionInitialized = false;
// Guards the once-only init of SessionId / ProcessTag / stream paths against
// concurrent first-callers from background threads. The trace API is mostly
// invoked from the main thread, but the engine's net/asset workers can race
// to log a startup line before the main thread reaches D_DoomMain's explicit
// InitSession(). Without this lock two callers could each generate a fresh
// SessionId and the later writers would see the loser's value.
static std::mutex SessionInitMutex;

static FString StreamPath;
static FString LatestStreamPath;
static FILE* StreamFile = nullptr;
static uint64_t LastStreamFlushMS = 0u;
// Stamp of the last CopyFileToLatest() call; gated by LatestStreamMirrorIntervalMS
// in the per-Warning severity-flush path so a steady stream of warnings cannot
// turn the playsim thread into a synchronous file-copier. Errors and explicit
// flushes still mirror immediately and overwrite this stamp.
static uint64_t LastLatestCopyMS = 0u;
static bool StreamHeaderWritten = false;

static std::unordered_map<std::string, size_t> ChannelCounts;
static std::mutex StatsMutex;
static std::atomic<uint64_t> TotalCount = {0};
static std::atomic<uint64_t> SeverityCounts[4] = {{0}, {0}, {0}, {0}};

static size_t GetTraceCapacity()
{
	return static_cast<size_t>(clamp<int>(SafeInt(debugtrace_capacity, int(DefaultTraceCapacity)), int(MinTraceCapacity), int(MaxTraceCapacity)));
}

static void EnsureTraceBufferCapacity()
{
	const size_t want = GetTraceCapacity();
	if (TraceBuffer.size() == want)
		return;

	TraceBuffer.resize(want);
	for (auto& entry : TraceBuffer)
		entry.Serial = 0;
}

static uint32_t GenerateSessionId()
{
	const uint64_t ms = I_msTime();
#ifdef _WIN32
	const uint32_t pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
	const uint32_t pid = static_cast<uint32_t>(getpid());
#endif
	return static_cast<uint32_t>(ms ^ (static_cast<uint64_t>(pid) << 16u) ^ pid);
}

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

static bool ChannelMatchesSingleFilter(const char* channel, const FString& filterChannel)
{
	if (filterChannel.IsEmpty() || channel == nullptr)
		return false;

	if (filterChannel.CompareNoCase(channel) == 0)
		return true;

	// Prefix: filter "net" matches "net.in", "net.out", etc.
	if (filterChannel.Len() < ChannelSize - 2)
	{
		FString prefix = filterChannel;
		prefix += '.';
		const size_t prefixLen = prefix.Len();
		if (strncmp(channel, prefix.GetChars(), prefixLen) == 0)
			return true;
	}

	return false;
}

static bool ChannelMatchesFilter(const char* channel, const char* channelFilter)
{
	if (IsAllChannelFilter(channelFilter))
		return true;

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
		if (ChannelMatchesSingleFilter(channel, filterChannel))
			return true;

		start = (*end != '\0') ? end + 1 : end;
	}

	return !hadFilter;
}

static bool ShouldChannelBeLogged(const char* channel)
{
	if (!SafeBool(debugtrace_enable, true))
		return false;
	return ChannelMatchesFilter(channel, SafeString(debugtrace_filter, ""));
}

static bool ShouldSeverityBeLogged(DebugTrace::Severity severity)
{
	if (!SafeBool(debugtrace_enable, true))
		return false;
	const int minSeverity = clamp<int>(SafeInt(debugtrace_minseverity, 0), 0, 3);
	return static_cast<int>(severity) >= minSeverity;
}

static void UpdateStatistics(const char* channel, DebugTrace::Severity severity)
{
	if (!SafeBool(debugtrace_stats, false))
		return;

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

static void CloseStreamFile()
{
	if (StreamFile != nullptr)
	{
		fflush(StreamFile);
		fclose(StreamFile);
		StreamFile = nullptr;
	}
	StreamHeaderWritten = false;
}

static void CopyFileToLatest()
{
	if (StreamPath.IsEmpty() || LatestStreamPath.IsEmpty())
		return;

	FILE* in = fopen(StreamPath.GetChars(), "rb");
	if (in == nullptr)
		return;

	FILE* out = fopen(LatestStreamPath.GetChars(), "wb");
	if (out == nullptr)
	{
		fclose(in);
		return;
	}

	char buffer[4096];
	size_t n = 0;
	while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)
		fwrite(buffer, 1, n, out);

	fclose(in);
	fclose(out);
}

static void RotateStreamFiles()
{
	if (StreamPath.IsEmpty())
		return;

	CloseStreamFile();
	CopyFileToLatest();
	LastLatestCopyMS = I_msTime();

	const int rotateCount = clamp<int>(SafeInt(debugtrace_stream_rotate_count, 4), 1, 32);
	const FString base = StreamPath;

	for (int i = rotateCount; i >= 1; --i)
	{
		FString older = FStringf("%s.%d.log", base.GetChars(), i);
		if (i == rotateCount)
			remove(older.GetChars());
		else
		{
			FString newer = FStringf("%s.%d.log", base.GetChars(), i + 1);
			remove(newer.GetChars());
			rename(older.GetChars(), newer.GetChars());
		}
	}

	FString first = FStringf("%s.1.log", base.GetChars());
	remove(first.GetChars());
	rename(StreamPath.GetChars(), first.GetChars());
}

static void RebuildStreamPaths();

// Called from inside StoreEntry -> WriteStreamLine -> OpenStreamFileIfNeeded
// to lazy-init the session for the very first trace line. The caller already
// holds TraceMutex; we additionally take SessionInitMutex with double-checked
// locking so a concurrent worker-thread trace cannot race the main thread's
// explicit InitSession() call and produce two different SessionIds.
//
// Lock-order discipline: TraceMutex -> SessionInitMutex is the only place
// these two locks nest. Public InitSession() / SetProcessTag() acquire
// SessionInitMutex first and release it BEFORE taking TraceMutex, so the
// pair is never held in the opposite order anywhere. The two orderings
// (T+S vs S only / S then T) are deadlock-free.
static void EnsureSessionForStream()
{
	if (SessionInitialized)
		return;

	std::lock_guard<std::mutex> lock(SessionInitMutex);
	if (SessionInitialized)
		return;

	if (SessionId == 0u)
		SessionId = GenerateSessionId();
	RebuildStreamPaths();
	SessionInitialized = true;
}

static bool OpenStreamFileIfNeeded()
{
	if (!SafeBool(debugtrace_stream, true))
		return false;
	EnsureSessionForStream();
	if (StreamPath.IsEmpty())
		return false;

	if (StreamFile != nullptr)
	{
		const long pos = ftell(StreamFile);
		if (pos >= 0)
		{
			const size_t rotateBytes = static_cast<size_t>(clamp<int>(SafeInt(debugtrace_stream_rotate_mb, 10), 1, 1024)) * 1024u * 1024u;
			if (static_cast<size_t>(pos) >= rotateBytes)
				RotateStreamFiles();
		}
	}

	if (StreamFile == nullptr)
	{
		StreamFile = fopen(StreamPath.GetChars(), "a");
		if (StreamFile == nullptr)
			return false;

		static char streamBuffer[8192];
		setvbuf(StreamFile, streamBuffer, _IOFBF, sizeof(streamBuffer));
		StreamHeaderWritten = false;
	}

	if (!StreamHeaderWritten)
	{
		fprintf(StreamFile,
			"HCDE Debug Trace Stream\n"
			"session=%08X process=%s capacity=%zu message=%zu\n\n",
			unsigned(SessionId), ProcessTag, TraceBuffer.size(), MessageSize);
		StreamHeaderWritten = true;
	}

	return StreamFile != nullptr;
}

static void WriteStreamLine(const TraceSnapshot& snapshot, bool forceFlush)
{
	if (!OpenStreamFileIfNeeded())
		return;

	fprintf(StreamFile, "[%10llu ms][sess=%08X][%-7s][0x%08llX][%5s] %s: %s\n",
		static_cast<unsigned long long>(snapshot.Milliseconds),
		unsigned(SessionId),
		ProcessTag,
		static_cast<unsigned long long>(snapshot.ThreadStamp & 0xffffffffull),
		SeverityName(snapshot.SeverityLevel),
		snapshot.Channel,
		snapshot.Message);

	const uint64_t nowMS = I_msTime();
	const bool isError = snapshot.SeverityLevel == DebugTrace::Severity::Error;
	const bool isWarning = snapshot.SeverityLevel == DebugTrace::Severity::Warning;
	const bool severityFlush = isError || isWarning;
	if (forceFlush || severityFlush || nowMS - LastStreamFlushMS >= StreamFlushIntervalMS)
	{
		fflush(StreamFile);
		LastStreamFlushMS = nowMS;
		// Mirror to "latest.log" only on Errors (always, since errors are rare and
		// usually post-mortem signals) or on a periodic timer for Warnings (so a
		// pathological mismatch loop cannot pin the playsim thread on file I/O).
		// Info/Debug entries never trigger a mirror; the explicit FlushStream(),
		// stream rotation, and shutdown paths take care of the final sync.
		if (isError || (isWarning && (LastLatestCopyMS == 0u || nowMS - LastLatestCopyMS >= LatestStreamMirrorIntervalMS)))
		{
			CopyFileToLatest();
			LastLatestCopyMS = nowMS;
		}
	}
}

static void StoreEntry(const char* channel, DebugTrace::Severity severity, const char* message)
{
	if (!SafeBool(debugtrace_enable, true))
		return;
	if (!ShouldChannelBeLogged(channel))
		return;
	if (!ShouldSeverityBeLogged(severity))
		return;

	UpdateStatistics(channel, severity);

	TraceSnapshot streamSnapshot = {};
	streamSnapshot.Milliseconds = I_msTime();
	streamSnapshot.ThreadStamp = GetThreadStamp();
	streamSnapshot.SeverityLevel = severity;
	std::snprintf(streamSnapshot.Channel, sizeof(streamSnapshot.Channel), "%s", channel != nullptr ? channel : "trace");
	std::snprintf(streamSnapshot.Message, sizeof(streamSnapshot.Message), "%s", message != nullptr ? message : "");

	std::lock_guard<std::mutex> lock(TraceMutex);
	EnsureTraceBufferCapacity();
	if (TraceBuffer.empty())
		return;

	const size_t capacity = TraceBuffer.size();
	const uint64_t serial = NextSerial.fetch_add(1, std::memory_order_relaxed);
	TraceEntry& entry = TraceBuffer[(serial - 1u) % capacity];
	entry.Serial = 0;

	entry.Milliseconds = streamSnapshot.Milliseconds;
	entry.ThreadStamp = streamSnapshot.ThreadStamp;
	entry.SeverityLevel = severity;
	std::snprintf(entry.Channel, sizeof(entry.Channel), "%s", streamSnapshot.Channel);
	std::snprintf(entry.Message, sizeof(entry.Message), "%s", streamSnapshot.Message);

	entry.Serial = serial;

	const bool forceFlush = static_cast<int>(severity) >= static_cast<int>(DebugTrace::Severity::Warning);
	WriteStreamLine(streamSnapshot, forceFlush);
}

static bool ReadEntrySnapshot(uint64_t serial, const char* channelFilter, DebugTrace::Severity minSeverity, TraceSnapshot& snapshot)
{
	std::lock_guard<std::mutex> lock(TraceMutex);
	if (TraceBuffer.empty())
		return false;

	const size_t capacity = TraceBuffer.size();
	const TraceEntry& entry = TraceBuffer[(serial - 1u) % capacity];
	const uint64_t published = entry.Serial;
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
	Printf("[%10llu ms][sess=%08X][%-7s][0x%08llX][%5s] %s: %s\n",
		static_cast<unsigned long long>(snapshot.Milliseconds),
		unsigned(SessionId),
		ProcessTag,
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

static void RebuildStreamPaths()
{
	const FString appData = M_GetAppDataPath(true);
	StreamPath = FStringf("%s/hcde_trace.%s.%08X.log", appData.GetChars(), ProcessTag, unsigned(SessionId));
	LatestStreamPath = FStringf("%s/hcde_trace.%s.latest.log", appData.GetChars(), ProcessTag);
}
}

namespace DebugTrace
{
void InitSession()
{
	// Fast path: already initialized. Reading the plain bool here is racy in
	// theory, but if a concurrent first-caller is still inside the slow path
	// we will simply contend on SessionInitMutex below instead of double
	// initing. Once `SessionInitialized` has been observed as true under the
	// lock it stays true for the lifetime of the process.
	if (SessionInitialized)
		return;

	std::lock_guard<std::mutex> lock(SessionInitMutex);
	if (SessionInitialized)
		return;

	SessionId = GenerateSessionId();
	std::snprintf(ProcessTag, sizeof(ProcessTag), "%s", "hcde");
	EnsureTraceBufferCapacity();
	RebuildStreamPaths();
	SessionInitialized = true;
}

void SetProcessTag(const char* tag)
{
	if (!SessionInitialized)
		InitSession();

	if (tag == nullptr || *tag == '\0')
		return;

	if (std::strncmp(ProcessTag, tag, sizeof(ProcessTag)) == 0)
		return;

	// Lock order: TraceMutex only. SessionInitMutex is never held alongside
	// TraceMutex anywhere - the public init/setter entry points (this one
	// included) finish their SessionInitMutex critical section before they
	// take TraceMutex, and the trace-time EnsureSessionForStream path never
	// touches SessionInitMutex. This avoids any AB/BA deadlock possibility.
	std::lock_guard<std::mutex> lock(TraceMutex);
	CloseStreamFile();
	std::snprintf(ProcessTag, sizeof(ProcessTag), "%.15s", tag);
	RebuildStreamPaths();
}

uint32_t GetSessionId()
{
	InitSession();
	return SessionId;
}

const char* GetProcessTag()
{
	InitSession();
	return ProcessTag;
}

const char* GetStreamPath()
{
	InitSession();
	return StreamPath.GetChars();
}

const char* GetLatestStreamPath()
{
	InitSession();
	return LatestStreamPath.GetChars();
}

void FlushStream()
{
	std::lock_guard<std::mutex> lock(TraceMutex);
	if (StreamFile != nullptr)
	{
		fflush(StreamFile);
		const uint64_t nowMS = I_msTime();
		LastStreamFlushMS = nowMS;
		LastLatestCopyMS = nowMS;
		CopyFileToLatest();
	}
}

void RotateStream()
{
	std::lock_guard<std::mutex> lock(TraceMutex);
	RotateStreamFiles();
}

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
	EnsureTraceBufferCapacity();
	for (auto& entry : TraceBuffer)
	{
		entry.Serial = 0;
		entry.Milliseconds = 0;
		entry.ThreadStamp = 0;
		entry.SeverityLevel = Severity::Info;
		entry.Channel[0] = '\0';
		entry.Message[0] = '\0';
	}

	NextSerial.store(1, std::memory_order_release);
}

void Mark(const char* channel, const char* message)
{
	StoreEntry(channel, Severity::Info, message);
}

void Markf(const char* channel, const char* format, ...)
{
	if (!SafeBool(debugtrace_enable, true))
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
	if (!SafeBool(debugtrace_enable, true))
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
	if (!SafeBool(debugtrace_enable, true))
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
	if (!SafeBool(debugtrace_enable, true))
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
	if (!SafeBool(debugtrace_enable, true))
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

	const size_t capacity = GetTraceCapacity();
	const uint64_t firstSerial = (lastSerial > capacity) ? (lastSerial - capacity + 1u) : 1u;

	FString header = "Recent engine trace";
	if (channelFilter != nullptr && *channelFilter != '\0')
		header.AppendFormat(" [channel=%s]", channelFilter);
	if (minSeverity != Severity::Debug)
		header.AppendFormat(" [severity=%s]", SeverityName(minSeverity));
	header.AppendFormat(" [sess=%08X][%s]", unsigned(SessionId), ProcessTag);

	Printf("%s:\n", header.GetChars());
	for (uint64_t serial = firstSerial; serial <= lastSerial; ++serial)
		PrintEntry(serial, channelFilter, minSeverity);
}

void WriteCrashInfo(char* buffer, size_t bufflen, const char* lfstr)
{
	if (!SafeBool(debugtrace_enable, true) || buffer == nullptr || bufflen == 0u)
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

	const size_t capacity = GetTraceCapacity();
	const uint64_t firstSerial = (lastSerial > capacity) ? (lastSerial - capacity + 1u) : 1u;
	AppendTraceText(buffer, end, "%sRecent engine trace [sess=%08X][%s]:",
		lfstr != nullptr ? lfstr : "\n", unsigned(SessionId), ProcessTag);
	for (uint64_t serial = firstSerial; serial <= lastSerial && buffer < end; ++serial)
	{
		TraceSnapshot snapshot;
		if (!ReadEntrySnapshot(serial, nullptr, Severity::Debug, snapshot))
			continue;

		AppendTraceText(buffer, end, "%s[%10llu ms][sess=%08X][%-7s][0x%08llX][%5s] %s: %s",
			lfstr != nullptr ? lfstr : "\n",
			static_cast<unsigned long long>(snapshot.Milliseconds),
			unsigned(SessionId),
			ProcessTag,
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
	Printf("  Session: %08X  Process: %s\n", unsigned(SessionId), ProcessTag);
	Printf("\n  By Severity:\n");
	Printf("    DEBUG:  %llu\n", static_cast<unsigned long long>(SeverityCounts[0].load(std::memory_order_relaxed)));
	Printf("    INFO:   %llu\n", static_cast<unsigned long long>(SeverityCounts[1].load(std::memory_order_relaxed)));
	Printf("    WARN:   %llu\n", static_cast<unsigned long long>(SeverityCounts[2].load(std::memory_order_relaxed)));
	Printf("    ERROR:  %llu\n", static_cast<unsigned long long>(SeverityCounts[3].load(std::memory_order_relaxed)));

	Printf("\n  By Channel:\n");
	for (const auto& pair : ChannelCounts)
		Printf("    %s: %zu\n", pair.first.c_str(), pair.second);
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

	const size_t capacity = GetTraceCapacity();
	fprintf(file, "HCDE Debug Trace Export\n");
	fprintf(file, "========================\n");
	fprintf(file, "session=%08X process=%s\n", unsigned(SessionId), ProcessTag);
	fprintf(file, "Total entries: %llu\n", static_cast<unsigned long long>(lastSerial));
	if (!IsAllChannelFilter(channelFilter))
		fprintf(file, "Channel filter: %s\n", channelFilter);
	if (minSeverity != Severity::Debug)
		fprintf(file, "Minimum severity: %s\n", SeverityName(minSeverity));
	fprintf(file, "\n");

	const uint64_t firstSerial = (lastSerial > capacity) ? (lastSerial - capacity + 1u) : 1u;
	for (uint64_t serial = firstSerial; serial <= lastSerial; ++serial)
	{
		TraceSnapshot snapshot;
		if (!ReadEntrySnapshot(serial, channelFilter, minSeverity, snapshot))
			continue;

		fprintf(file, "[%10llu ms][sess=%08X][%-7s][0x%08llX][%5s] %s: %s\n",
			static_cast<unsigned long long>(snapshot.Milliseconds),
			unsigned(SessionId),
			ProcessTag,
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
		const int level = clamp<int>(SafeInt(debugtrace_minseverity, 0), 0, 3);
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
		Printf("Current channel filter: %s\n", SafeString(debugtrace_filter, ""));
		Printf("Empty filter means all channels are logged.\n");
		Printf("Multiple channels can be separated by commas.\n");
		Printf("Prefix match: filter \"net\" matches net.in, net.out, etc.\n");
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

CCMD(debugtraceflush)
{
	DebugTrace::FlushStream();
	Printf("Debug trace stream flushed.\n");
}

CCMD(debugtraceopen)
{
	DebugTrace::InitSession();
	Printf("Debug trace stream: %s\n", DebugTrace::GetStreamPath());
	Printf("Debug trace latest: %s\n", DebugTrace::GetLatestStreamPath());
	Printf("Session: %08X  Process: %s\n", unsigned(DebugTrace::GetSessionId()), DebugTrace::GetProcessTag());
}

CCMD(debugtracerotate)
{
	DebugTrace::RotateStream();
	Printf("Debug trace stream rotated.\n");
	Printf("Active stream: %s\n", DebugTrace::GetStreamPath());
	Printf("Latest copy: %s\n", DebugTrace::GetLatestStreamPath());
}
