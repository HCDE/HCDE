/*
** debugtrace.h
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

#pragma once

#include <cstddef>

namespace DebugTrace
{
	// Severity levels for debug trace messages
	enum class Severity
	{
		Debug = 0,
		Info = 1,
		Warning = 2,
		Error = 3
	};

	// Core functions
	void Clear();
	void Mark(const char* channel, const char* message);
	void Markf(const char* channel, const char* format, ...);
	
	// Severity-specific functions
	void Debug(const char* channel, const char* message);
	void Debugf(const char* channel, const char* format, ...);
	void Info(const char* channel, const char* message);
	void Infof(const char* channel, const char* format, ...);
	void Warning(const char* channel, const char* message);
	void Warningf(const char* channel, const char* format, ...);
	void Error(const char* channel, const char* message);
	void Errorf(const char* channel, const char* format, ...);
	
	// Dump and crash info
	void Dump();
	void Dump(const char* channelFilter); // Dump only specific channel
	void Dump(Severity minSeverity); // Dump only certain severity and higher
	void Dump(const char* channelFilter, Severity minSeverity); // Both filters
	void WriteCrashInfo(char* buffer, size_t bufflen, const char* lfstr);
	
	// Statistics
	void ClearStats();
	void DumpStats();
	size_t GetTotalCount();
	size_t GetChannelCount(const char* channel);
	
	// File output
	bool SaveToFile(const char* filename);
	bool SaveToFile(const char* filename, const char* channelFilter, Severity minSeverity);
	
	// Filtering
	bool IsChannelEnabled(const char* channel);
	bool IsSeverityEnabled(Severity severity);
	bool ParseSeverity(const char* text, Severity& severity);
	const char* GetSeverityName(Severity severity);
}
