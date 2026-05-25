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
** Channel taxonomy (use these names consistently):
**   net.in          - inbound packet receive/dispatch
**   net.out         - outbound HSendPacket (1Hz aggregate for heartbeats)
**   net.session     - setup, connect, kick, quitter promotion
**   net.snapshot    - server snapshot apply/reject
**   net.command     - client command send/recv (1Hz aggregate)
**   net.cap         - HCDE live capability negotiation
**   net.levelstart  - LST_WAITING/HOST/READY transitions
**   mode            - sv_gametype, deathmatch, teamplay mutations
**   invasion        - invasion state machine transitions
**   invasion.wave   - wave director budget/spawn/completion
**   invasion.mirror - mirror spawn/despawn/blocking
**   predict         - prediction faults, repairs, replay pressure
**   game            - gameaction / gamestate transitions
**   level           - G_InitNew, G_DoLoadLevel, G_ExitLevel, travel
**   player          - join, leave, spectate, ready, slot
**   input           - G_BuildTiccmd aggregate (1Hz)
**   playsim.actor   - tracked/replicated actor spawn/despawn
**   playsim.damage  - damage crossing player boundary
**   playsim.pickup  - pickup events
**   playsim.projectile - replicated projectile spawn/destroy
**   pause           - pause/unpause
**   console         - console open/close
**   startup         - D_DoomMain phases
**   main            - D_DoomLoop lifecycle
**   netui           - launcher net start window
**   serv            - hcdeserv-only runtime events
**
** Filter: debugtrace_filter accepts comma-separated channels; a filter of
** "net" matches any channel starting with "net." or equal to "net".
**
*/

#pragma once

#include <cstddef>
#include <cstdint>

namespace DebugTrace
{
	enum class Severity
	{
		Debug = 0,
		Info = 1,
		Warning = 2,
		Error = 3
	};

	void InitSession();
	void SetProcessTag(const char* tag);

	uint32_t GetSessionId();
	const char* GetProcessTag();
	const char* GetStreamPath();
	const char* GetLatestStreamPath();

	void Clear();
	void Mark(const char* channel, const char* message);
	void Markf(const char* channel, const char* format, ...);

	void Debug(const char* channel, const char* message);
	void Debugf(const char* channel, const char* format, ...);
	void Info(const char* channel, const char* message);
	void Infof(const char* channel, const char* format, ...);
	void Warning(const char* channel, const char* message);
	void Warningf(const char* channel, const char* format, ...);
	void Error(const char* channel, const char* message);
	void Errorf(const char* channel, const char* format, ...);

	void Dump();
	void Dump(const char* channelFilter);
	void Dump(Severity minSeverity);
	void Dump(const char* channelFilter, Severity minSeverity);
	void WriteCrashInfo(char* buffer, size_t bufflen, const char* lfstr);

	void ClearStats();
	void DumpStats();
	size_t GetTotalCount();
	size_t GetChannelCount(const char* channel);

	bool SaveToFile(const char* filename);
	bool SaveToFile(const char* filename, const char* channelFilter, Severity minSeverity);

	void FlushStream();
	void RotateStream();

	bool IsChannelEnabled(const char* channel);
	bool IsSeverityEnabled(Severity severity);
	bool ParseSeverity(const char* text, Severity& severity);
	const char* GetSeverityName(Severity severity);
}
