/*
** hcde_servermode.cpp
**
** HCDE dedicated-server runtime boundary.
**
**-----------------------------------------------------------------------------
**
** Copyright 2026 HCDE Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**-----------------------------------------------------------------------------
*/

#include "hcde_servermode.h"

#include <stdlib.h>

#include "debugtrace.h"
#include "engineerrors.h"
#include "i_net.h"
#include "m_argv.h"
#include "printf.h"
#include "zstring.h"

EXTERN_FARG(iwad);
EXTERN_FARG(map);
EXTERN_FARG(port);
EXTERN_FARG(warp);

namespace
{
struct FHCDEServerRuntime
{
	bool Initialized = false;
	bool DedicatedExecutable = false;
	bool DedicatedServer = false;
	bool HostMode = false;
	bool JoinMode = false;
	bool DedicatedJoin = false;
	bool NetWaitSilent = false;
	bool HiddenServerSlot = false;
	bool MasterStateKnown = false;
	bool MasterAdvertisingEnabled = false;
	size_t ConfiguredMasters = 0;
	int RequestedServerSlots = 0;
	int VisibleSlots = 1;
	int InternalSlots = 1;
	int GamePort = 0;
	FString IWADArg;
	FString SelectedIWAD;
	FString MapArg;
	FString JoinAddress;
	FString FlowState;
};

FHCDEServerRuntime Runtime;

bool IsValueArg(int index)
{
	return Args != nullptr
		&& index > 0
		&& index < Args->NumArgs()
		&& Args->GetArg(index) != nullptr
		&& Args->GetArg(index)[0] != '-'
		&& Args->GetArg(index)[0] != '+';
}

int ReadIntValue(const FArg& arg)
{
	const int index = Args != nullptr ? Args->CheckParm(arg) : 0;
	if (!IsValueArg(index + 1))
		return 0;
	return atoi(Args->GetArg(index + 1));
}

void AssignArgValue(FString& out, const FArg& arg)
{
	const char* value = Args != nullptr ? Args->CheckValue(arg) : nullptr;
	out = value != nullptr ? value : "";
}

FString ReadWarpValue()
{
	FString text;
	const int index = Args != nullptr ? Args->CheckParm(FArg_warp) : 0;
	if (!IsValueArg(index + 1))
		return text;

	text = Args->GetArg(index + 1);
	if (IsValueArg(index + 2))
	{
		text.AppendFormat(" %s", Args->GetArg(index + 2));
	}
	return text;
}

const char* TextOr(const FString& text, const char* fallback)
{
	return text.IsNotEmpty() ? text.GetChars() : fallback;
}

const char* YesNo(bool value)
{
	return value ? "yes" : "no";
}
}

void HCDE_ServerMode_InitFromArgs()
{
	if (Runtime.Initialized)
		return;

#ifdef HCDE_DEDICATED_SERVER
	Runtime.DedicatedExecutable = true;
#else
	Runtime.DedicatedExecutable = false;
#endif

	if (Args == nullptr)
		return;

	Runtime.Initialized = true;
	Runtime.DedicatedServer = Args->CheckParm(FArg_server) != 0;
	Runtime.HostMode = Args->CheckParm(FArg_host) != 0;
	Runtime.JoinMode = Args->CheckParm(FArg_join) != 0;
	Runtime.DedicatedJoin = Args->CheckParm(FArg_dedicatedjoin) != 0;
	Runtime.NetWaitSilent = Args->CheckParm(FArg_netwaitsilent) != 0;
	Runtime.RequestedServerSlots = ReadIntValue(FArg_server);
	Runtime.GamePort = ReadIntValue(FArg_port);
	AssignArgValue(Runtime.IWADArg, FArg_iwad);
	AssignArgValue(Runtime.MapArg, FArg_map);
	if (Runtime.MapArg.IsEmpty())
		Runtime.MapArg = ReadWarpValue();
	AssignArgValue(Runtime.JoinAddress, FArg_join);
	Runtime.FlowState = "startup";

	if (Runtime.DedicatedExecutable && !Runtime.DedicatedServer)
	{
		I_FatalError("hcdeserv was started without -server <slots>. Dedicated server builds must enter the explicit server runtime.");
	}

	DebugTrace::Markf("servermode", "init dedicated=%d executable=%d join=%d silent=%d",
		Runtime.DedicatedServer ? 1 : 0,
		Runtime.DedicatedExecutable ? 1 : 0,
		Runtime.DedicatedJoin ? 1 : 0,
		Runtime.NetWaitSilent ? 1 : 0);
}

void HCDE_ServerMode_SetSelectedIWAD(const char* iwadName)
{
	HCDE_ServerMode_InitFromArgs();
	Runtime.SelectedIWAD = iwadName != nullptr ? iwadName : "";
}

void HCDE_ServerMode_SetNetworkDetails(int visibleSlots, int internalSlots, int gamePort, bool hiddenServerSlot, const char* flowState)
{
	HCDE_ServerMode_InitFromArgs();
	Runtime.VisibleSlots = visibleSlots;
	Runtime.InternalSlots = internalSlots;
	Runtime.GamePort = gamePort;
	Runtime.HiddenServerSlot = hiddenServerSlot;
	Runtime.FlowState = flowState != nullptr ? flowState : "";
}

void HCDE_ServerMode_SetMasterState(bool advertisingEnabled, size_t configuredMasters)
{
	HCDE_ServerMode_InitFromArgs();
	Runtime.MasterStateKnown = true;
	Runtime.MasterAdvertisingEnabled = advertisingEnabled;
	Runtime.ConfiguredMasters = configuredMasters;
}

void HCDE_ServerMode_PrintDiagnostics(const char* phase)
{
	HCDE_ServerMode_InitFromArgs();
	if (!(Runtime.DedicatedServer || Runtime.HostMode || Runtime.JoinMode || Runtime.DedicatedJoin))
		return;

	const char* masterState = Runtime.MasterStateKnown
		? (Runtime.MasterAdvertisingEnabled ? "enabled" : "disabled")
		: "not-initialized";

	Printf("HCDE server runtime [%s]: mode=%s executable=%s host=%s join=%s dedicatedjoin=%s silent-room-ui=%s\n",
		phase != nullptr ? phase : "unknown",
		Runtime.DedicatedServer ? "dedicated-server" : (Runtime.JoinMode ? "join-client" : (Runtime.HostMode ? "host-client" : "client")),
		YesNo(Runtime.DedicatedExecutable),
		YesNo(Runtime.HostMode),
		YesNo(Runtime.JoinMode),
		YesNo(Runtime.DedicatedJoin),
		YesNo(HCDE_ServerMode_ShouldSuppressRoomUI()));
	Printf("HCDE server runtime [%s]: requested-slots=%d visible-slots=%d internal-slots=%d hidden-server-slot=%s port=%s flow=%s\n",
		phase != nullptr ? phase : "unknown",
		Runtime.RequestedServerSlots,
		Runtime.VisibleSlots,
		Runtime.InternalSlots,
		YesNo(Runtime.HiddenServerSlot),
		Runtime.GamePort > 0 ? FStringf("%d", Runtime.GamePort).GetChars() : "default",
		TextOr(Runtime.FlowState, "unknown"));
	Printf("HCDE server runtime [%s]: iwad-arg=%s selected-iwad=%s map=%s join-address=%s master=%s masters=%zu startup-ui=%s\n",
		phase != nullptr ? phase : "unknown",
		TextOr(Runtime.IWADArg, "auto"),
		TextOr(Runtime.SelectedIWAD, "unknown"),
		TextOr(Runtime.MapArg, "default"),
		TextOr(Runtime.JoinAddress, "none"),
		masterState,
		Runtime.ConfiguredMasters,
		YesNo(!HCDE_ServerMode_ShouldSuppressStartupUI()));
}

void HCDE_ServerMode_GuardClientSubsystem(const char* subsystem)
{
	HCDE_ServerMode_InitFromArgs();
	if (!Runtime.DedicatedServer)
		return;

	I_FatalError("Dedicated server attempted to initialize client subsystem: %s. This is a server-boundary bug.",
		subsystem != nullptr ? subsystem : "unknown");
}

bool HCDE_ServerMode_IsDedicatedServer()
{
	HCDE_ServerMode_InitFromArgs();
	return Runtime.DedicatedServer;
}

bool HCDE_ServerMode_IsDedicatedExecutable()
{
	HCDE_ServerMode_InitFromArgs();
	return Runtime.DedicatedExecutable;
}

bool HCDE_ServerMode_IsDedicatedJoin()
{
	HCDE_ServerMode_InitFromArgs();
	return Runtime.DedicatedJoin;
}

bool HCDE_ServerMode_ShouldSuppressRoomUI()
{
	HCDE_ServerMode_InitFromArgs();
	return Runtime.DedicatedServer || Runtime.NetWaitSilent || Runtime.DedicatedJoin || Runtime.JoinMode;
}

bool HCDE_ServerMode_ShouldSuppressStartupUI()
{
	HCDE_ServerMode_InitFromArgs();
	return Runtime.DedicatedServer;
}
