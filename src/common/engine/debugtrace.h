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
	void Clear();
	void Mark(const char* channel, const char* message);
	void Markf(const char* channel, const char* format, ...);
	void Dump();
	void WriteCrashInfo(char* buffer, size_t bufflen, const char* lfstr);
}
