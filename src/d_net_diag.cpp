#include "d_net.h"
#include "d_player.h"
#include "g_level.h"
#include "doomstat.h"
#include "common/engine/debugtrace.h"
#include "d_net_diag.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "common/console/c_console.h"
#include "i_time.h"
#include "d_event.h"
#include "i_specialpaths.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

EXTERN_CVAR(Int, net_echo_debug)

// Implement Net_LogPingSample
void Net_LogPingSample(int clientNum, int leadTics, int leadMs, int rttMs, int ticDup, int extraTics, int delta)
{
	DebugTrace::Debugf("net.ping", "client=%d lead_tics=%d lead_ms=%d rtt_ms=%d TicDup=%d extratics=%d delta=%d",
		clientNum, leadTics, leadMs, rttMs, ticDup, extraTics, delta);
}

// Implement Net_CompareEchoToLocal
void Net_CompareEchoToLocal(int clientNum, uint32_t serverTic, int playerNum,
	uint32_t readyWeapName, uint32_t pendingWeapName,
	uint32_t pspriteStateName, int16_t pspriteTics,
	uint16_t weaponState, uint8_t playerState, int16_t viewHeight)
{
	if (playerNum < 0 || playerNum >= MAXPLAYERS)
		return;

	const player_t& player = players[playerNum];

	// Extract local values
	uint32_t localReadyWeapName = 0;
	if (player.ReadyWeapon != nullptr)
		localReadyWeapName = player.ReadyWeapon->GetClass()->TypeName.GetIndex();

	uint32_t localPendingWeapName = 0;
	if (player.PendingWeapon != nullptr && player.PendingWeapon != WP_NOCHANGE)
		localPendingWeapName = player.PendingWeapon->GetClass()->TypeName.GetIndex();
	else if (player.PendingWeapon == WP_NOCHANGE)
		localPendingWeapName = 0xFFFFFFFF;

	uint32_t localPspriteStateName = 0;
	int16_t localPspriteTics = 0;
	if (player.psprites != nullptr)
	{
		DPSprite* sp = const_cast<player_t&>(player).psprites;
		while (sp != nullptr && sp->ID != PSP_WEAPON)
		{
			sp = sp->Next;
		}
		if (sp != nullptr && sp->GetState() != nullptr)
		{
			localPspriteStateName = FName(FState::StaticGetStateName(sp->GetState())).GetIndex();
			localPspriteTics = sp->Tics;
		}
	}

	uint16_t localWeaponState = player.WeaponState;
	uint8_t localPlayerState = player.playerstate;
	int16_t localViewHeight = int16_t(player.viewheight);

	// Log desyncs
	bool desync = false;
	if (localReadyWeapName != readyWeapName)
	{
		desync = true;
		const char* sName = FName(ENamedName(readyWeapName)).IsValidName() ? FName(ENamedName(readyWeapName)).GetChars() : "None";
		const char* lName = FName(ENamedName(localReadyWeapName)).IsValidName() ? FName(ENamedName(localReadyWeapName)).GetChars() : "None";
		DebugTrace::Warningf("net.desync", "[WEAPON DESYNC] player=%d server ReadyWeapon=%s local ReadyWeapon=%s tic=%u",
			playerNum, sName, lName, serverTic);
	}
	if (localPendingWeapName != pendingWeapName)
	{
		desync = true;
		const char* sName = (pendingWeapName == 0xFFFFFFFF) ? "WP_NOCHANGE" : (FName(ENamedName(pendingWeapName)).IsValidName() ? FName(ENamedName(pendingWeapName)).GetChars() : "None");
		const char* lName = (localPendingWeapName == 0xFFFFFFFF) ? "WP_NOCHANGE" : (FName(ENamedName(localPendingWeapName)).IsValidName() ? FName(ENamedName(localPendingWeapName)).GetChars() : "None");
		DebugTrace::Warningf("net.desync", "[PENDING WEAPON DESYNC] player=%d server PendingWeapon=%s local PendingWeapon=%s tic=%u",
			playerNum, sName, lName, serverTic);
	}
	if (localPspriteStateName != pspriteStateName || (pspriteStateName != 0 && localPspriteTics != pspriteTics))
	{
		desync = true;
		const char* sName = FName(ENamedName(pspriteStateName)).IsValidName() ? FName(ENamedName(pspriteStateName)).GetChars() : "None";
		const char* lName = FName(ENamedName(localPspriteStateName)).IsValidName() ? FName(ENamedName(localPspriteStateName)).GetChars() : "None";
		DebugTrace::Warningf("net.desync", "[PSPRITE STATE DESYNC] player=%d server state=%s (tics=%d) local state=%s (tics=%d) tic=%u",
			playerNum, sName, pspriteTics, lName, localPspriteTics, serverTic);
	}
	if (localWeaponState != weaponState)
	{
		desync = true;
		DebugTrace::Warningf("net.desync", "[WEAPON STATE DESYNC] player=%d server WeaponState=0x%04x local WeaponState=0x%04x tic=%u",
			playerNum, weaponState, localWeaponState, serverTic);
	}
	if (localPlayerState != playerState)
	{
		desync = true;
		DebugTrace::Warningf("net.desync", "[PLAYER STATE DESYNC] player=%d server playerstate=%d local playerstate=%d tic=%u",
			playerNum, playerState, localPlayerState, serverTic);
	}
	if (abs(localViewHeight - (viewHeight / 256.0)) > 1.0)
	{
		desync = true;
		DebugTrace::Warningf("net.desync", "[VIEWHEIGHT DESYNC] player=%d server viewheight=%.2f local viewheight=%.2f tic=%u",
			playerNum, viewHeight / 256.0, double(localViewHeight), serverTic);
	}
}

// Append HCDEPresentationEcho
bool HCDEAppendPresentationEcho(int client, uint8_t* output, size_t outputCapacity, size_t& cursor, const uint8_t* playerNums, size_t playerCount)
{
	const size_t startCursor = cursor;
	if (!HCDEAppendBytes(output, outputCapacity, cursor, HCDEPresentationEchoMagic, sizeof(HCDEPresentationEchoMagic))
		|| !HCDEAppendByte(output, outputCapacity, cursor, HCDEPresentationEchoProtocolVersion)
		|| !HCDEAppendByte(output, outputCapacity, cursor, uint8_t(playerCount)))
	{
		return false;
	}

	for (size_t i = 0u; i < playerCount; ++i)
	{
		const uint8_t playerNum = playerNums[i];
		if (playerNum >= MAXPLAYERS)
			return false;

		const player_t& player = players[playerNum];

		uint32_t readyWeapNameIndex = 0;
		if (player.ReadyWeapon != nullptr)
			readyWeapNameIndex = player.ReadyWeapon->GetClass()->TypeName.GetIndex();

		uint32_t pendingWeapNameIndex = 0;
		if (player.PendingWeapon != nullptr && player.PendingWeapon != WP_NOCHANGE)
			pendingWeapNameIndex = player.PendingWeapon->GetClass()->TypeName.GetIndex();
		else if (player.PendingWeapon == WP_NOCHANGE)
			pendingWeapNameIndex = 0xFFFFFFFF; // sentinel for WP_NOCHANGE

		uint32_t pspriteStateNameIndex = 0;
		int16_t pspriteTics = 0;
		if (player.psprites != nullptr)
		{
			DPSprite* sp = const_cast<player_t&>(player).psprites;
			while (sp != nullptr && sp->ID != PSP_WEAPON)
			{
				sp = sp->Next;
			}
			if (sp != nullptr && sp->GetState() != nullptr)
			{
				pspriteStateNameIndex = FName(FState::StaticGetStateName(sp->GetState())).GetIndex();
				pspriteTics = sp->Tics;
			}
		}

		uint16_t weaponState = player.WeaponState;
		uint8_t playerState = player.playerstate;
		int16_t viewHeight = int16_t(clamp<int>(player.viewheight * 256.0, INT16_MIN, INT16_MAX));

		if (!HCDEAppendByte(output, outputCapacity, cursor, playerNum)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, readyWeapNameIndex)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, pendingWeapNameIndex)
			|| !HCDEAppendBE32(output, outputCapacity, cursor, pspriteStateNameIndex)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(pspriteTics))
			|| !HCDEAppendBE16(output, outputCapacity, cursor, weaponState)
			|| !HCDEAppendByte(output, outputCapacity, cursor, playerState)
			|| !HCDEAppendBE16(output, outputCapacity, cursor, uint16_t(viewHeight)))
		{
			return false;
		}
	}

	HCDERecordLiveLaneTx(HLANE_PRESENTATION_ECHO, client, cursor - startCursor);
	return true;
}

// Read HCDEPresentationEcho
bool HCDEReadPresentationEcho(int clientNum, const uint8_t* body, size_t bodyBytes, size_t& cursor)
{
	if (cursor > bodyBytes || bodyBytes - cursor < 6)
		return false;

	const size_t startCursor = cursor;

	if (memcmp(&body[cursor], HCDEPresentationEchoMagic, sizeof(HCDEPresentationEchoMagic)) != 0)
		return false;

	cursor += sizeof(HCDEPresentationEchoMagic);
	const uint8_t version = body[cursor++];
	const uint8_t playerCount = body[cursor++];

	if (version != HCDEPresentationEchoProtocolVersion)
		return false;

	uint32_t serverTic = max<int>(gametic, 0); // fallback or reference tic

	for (uint8_t i = 0u; i < playerCount; ++i)
	{
		if (cursor > bodyBytes || bodyBytes - cursor < 19)
			return false;

		uint8_t playerNum = body[cursor++];
		uint32_t readyWeapNameIndex = 0;
		uint32_t pendingWeapNameIndex = 0;
		uint32_t pspriteStateNameIndex = 0;
		uint16_t pspriteTicsRaw = 0;
		uint16_t weaponState = 0;
		uint8_t playerState = 0;
		uint16_t viewHeightRaw = 0;

		if (!HCDEReadBE32Field(body, bodyBytes, cursor, readyWeapNameIndex)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, pendingWeapNameIndex)
			|| !HCDEReadBE32Field(body, bodyBytes, cursor, pspriteStateNameIndex)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, pspriteTicsRaw)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, weaponState)
			|| !HCDEReadByteField(body, bodyBytes, cursor, playerState)
			|| !HCDEReadBE16Field(body, bodyBytes, cursor, viewHeightRaw))
		{
			return false;
		}

		int16_t pspriteTics = int16_t(pspriteTicsRaw);
		int16_t viewHeight = int16_t(viewHeightRaw);

		Net_CompareEchoToLocal(clientNum, serverTic, playerNum,
			readyWeapNameIndex, pendingWeapNameIndex,
			pspriteStateNameIndex, pspriteTics,
			weaponState, playerState, viewHeight);
	}

	HCDERecordLiveLaneRx(HLANE_PRESENTATION_ECHO, clientNum, cursor - startCursor);
	return true;
}

// CCMD(net_echo_dump)
CCMD(net_echo_dump)
{
	if (consoleplayer < 0 || consoleplayer >= MAXPLAYERS)
		return;

	const player_t& player = players[consoleplayer];

	uint32_t readyWeapNameIndex = 0;
	if (player.ReadyWeapon != nullptr)
		readyWeapNameIndex = player.ReadyWeapon->GetClass()->TypeName.GetIndex();

	uint32_t pendingWeapNameIndex = 0;
	if (player.PendingWeapon != nullptr && player.PendingWeapon != WP_NOCHANGE)
		pendingWeapNameIndex = player.PendingWeapon->GetClass()->TypeName.GetIndex();
	else if (player.PendingWeapon == WP_NOCHANGE)
		pendingWeapNameIndex = 0xFFFFFFFF;

	uint32_t pspriteStateNameIndex = 0;
	int16_t pspriteTics = 0;
	if (player.psprites != nullptr)
	{
		DPSprite* sp = const_cast<player_t&>(player).psprites;
		while (sp != nullptr && sp->ID != PSP_WEAPON)
		{
			sp = sp->Next;
		}
		if (sp != nullptr && sp->GetState() != nullptr)
		{
			pspriteStateNameIndex = FName(FState::StaticGetStateName(sp->GetState())).GetIndex();
			pspriteTics = sp->Tics;
		}
	}

	const char* readyName = FName(ENamedName(readyWeapNameIndex)).IsValidName() ? FName(ENamedName(readyWeapNameIndex)).GetChars() : "None";
	const char* pendingName = (pendingWeapNameIndex == 0xFFFFFFFF) ? "WP_NOCHANGE" : (FName(ENamedName(pendingWeapNameIndex)).IsValidName() ? FName(ENamedName(pendingWeapNameIndex)).GetChars() : "None");
	const char* pspStateName = FName(ENamedName(pspriteStateNameIndex)).IsValidName() ? FName(ENamedName(pspriteStateNameIndex)).GetChars() : "None";

	Printf("Local Echo Dump (player=%d, gametic=%d, ClientTic=%d):\n"
		"  ReadyWeapon: %s (0x%x)\n"
		"  PendingWeapon: %s (0x%x)\n"
		"  PSprite PSP_WEAPON state: %s (tics=%d, 0x%x)\n"
		"  WeaponState: 0x%04x\n"
		"  playerstate: %d\n"
		"  viewheight: %.2f\n",
		consoleplayer, gametic, ClientTic,
		readyName, readyWeapNameIndex,
		pendingName, pendingWeapNameIndex,
		pspStateName, pspriteTics, pspriteStateNameIndex,
		player.WeaponState,
		player.playerstate,
		player.viewheight);
}

#include <fstream>
#include <string>

EXTERN_CVAR(Int, net_self_test_run_client)

// Implement Net_ApplySelfTestInputs
void Net_ApplySelfTestInputs(usercmd_t* cmd, int clientTic)
{
	if (*net_self_test_run_client == 0)
		return;

	// Reset all cmd fields so it is 100% deterministic
	cmd->buttons = 0;
	cmd->pitch = 0;
	cmd->yaw = 0;
	cmd->roll = 0;
	cmd->forwardmove = 0;
	cmd->sidemove = 0;
	cmd->upmove = 0;

	// 35 tics = 1 second of Doom gameplay
	const int phase = clientTic % 250;

	if (phase < 35)
	{
		// 1. Move forward
		cmd->forwardmove = 0x3200;
	}
	else if (phase < 70)
	{
		// 2. Fire weapon
		cmd->buttons |= BT_ATTACK;
	}
	else if (phase < 105)
	{
		// 3. Switch weapon
		cmd->buttons |= BT_ZOOM;
	}
	else if (phase < 140)
	{
		// 4. Move backward
		cmd->forwardmove = -0x3200;
	}
	else if (phase < 175)
	{
		// 5. Die / Respawn
		cmd->buttons |= BT_USE;
	}
	else if (phase < 210)
	{
		// 6. Fire again
		cmd->buttons |= BT_ATTACK;
	}
	else
	{
		cmd->buttons |= BT_USE;
	}
}

// CCMD(net_self_test)
CCMD(net_self_test)
{
	Printf("Starting HCDE self-test harness...\n");

#ifdef _WIN32
	char exePath[MAX_PATH];
	if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0)
	{
		Printf("Self-test error: unable to get current executable path.\n");
		return;
	}
	std::string currentExe = exePath;
	size_t lastSlash = currentExe.find_last_of("\\/");
	std::string binDir = (lastSlash != std::string::npos) ? currentExe.substr(0, lastSlash + 1) : "";
	std::string serverExe = binDir + "hcdeserv.exe";

	Printf("Parent executable: %s\n", currentExe.c_str());
	Printf("Server executable: %s\n", serverExe.c_str());

	// Clean up any old log files first to avoid reading stale test runs
	const FString appData = M_GetAppDataPath(true);
	std::string clientLogPath = std::string(appData.GetChars()) + "/hcde_trace.hcde.latest.log";
	std::string serverLogPath = std::string(appData.GetChars()) + "/hcde_trace.hcdeserv.latest.log";
	std::remove(clientLogPath.c_str());
	std::remove(serverLogPath.c_str());

	// Spawn hcdeserv
	STARTUPINFOA siServer;
	memset(&siServer, 0, sizeof(siServer));
	siServer.cb = sizeof(siServer);

	PROCESS_INFORMATION piServer;
	memset(&piServer, 0, sizeof(piServer));

	std::string serverCmd = "\"" + serverExe + "\" -server 2 -port 5029 -iwad doom2.wad +map map01 +net_echo_debug 1 +debugtrace_enable 1";

	Printf("Spawning server process...\n");
	BOOL okServer = CreateProcessA(NULL, &serverCmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &siServer, &piServer);
	if (!okServer)
	{
		Printf("Self-test error: failed to spawn server (error code %lu).\n", GetLastError());
		return;
	}

	// Wait 2 seconds for server to start up and bind to port
	Sleep(2000);

	// Spawn client
	STARTUPINFOA siClient;
	memset(&siClient, 0, sizeof(siClient));
	siClient.cb = sizeof(siClient);

	PROCESS_INFORMATION piClient;
	memset(&piClient, 0, sizeof(piClient));

	std::string clientCmd = "\"" + currentExe + "\" -join 127.0.0.1:5029 -iwad doom2.wad +net_self_test_run_client 1 +net_echo_debug 1 +debugtrace_enable 1";

	Printf("Spawning client process...\n");
	BOOL okClient = CreateProcessA(NULL, &clientCmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &siClient, &piClient);
	if (!okClient)
	{
		Printf("Self-test error: failed to spawn client (error code %lu).\n", GetLastError());
		TerminateProcess(piServer.hProcess, 0);
		CloseHandle(piServer.hProcess);
		CloseHandle(piServer.hThread);
		return;
	}

	Printf("Running deterministic simulation for 12 seconds...\n");
	Sleep(12000);

	Printf("Terminating test processes...\n");
	TerminateProcess(piClient.hProcess, 0);
	TerminateProcess(piServer.hProcess, 0);

	CloseHandle(piClient.hProcess);
	CloseHandle(piClient.hThread);
	CloseHandle(piServer.hProcess);
	CloseHandle(piServer.hThread);

	Printf("Analyzing trace logs...\n");

	// Now parse logs
	std::ifstream clientFile(clientLogPath);
	std::ifstream serverFile(serverLogPath);

	if (!clientFile.is_open())
	{
		Printf("Self-test FAILED: Client trace log not found at %s.\n", clientLogPath.c_str());
		return;
	}
	if (!serverFile.is_open())
	{
		Printf("Self-test FAILED: Server trace log not found at %s.\n", serverLogPath.c_str());
		return;
	}

	bool desyncFound = false;
	std::string line;
	int desyncCount = 0;
	int clientEvents = 0;
	int serverEvents = 0;

	while (std::getline(clientFile, line))
	{
		if (line.find("playsim.psprite") != std::string::npos || line.find("playsim.playerstate") != std::string::npos)
			clientEvents++;

		if (line.find("[PLAYER STATE DESYNC]") != std::string::npos ||
			line.find("[WEAPON DESYNC]") != std::string::npos ||
			line.find("[PSPRITE DESYNC]") != std::string::npos ||
			line.find("[VIEWHEIGHT DESYNC]") != std::string::npos)
		{
			desyncFound = true;
			desyncCount++;
			Printf("Desync found in client log: %s\n", line.c_str());
		}
	}

	while (std::getline(serverFile, line))
	{
		if (line.find("playsim.psprite") != std::string::npos || line.find("playsim.playerstate") != std::string::npos)
			serverEvents++;
	}

	Printf("Test results summary:\n");
	Printf("  Client events logged: %d\n", clientEvents);
	Printf("  Server events logged: %d\n", serverEvents);
	Printf("  Desyncs detected: %d\n", desyncCount);

	if (clientEvents == 0)
	{
		Printf("Self-test FAILED: Client logged 0 simulation events. Connection may have failed.\n");
		return;
	}

	if (desyncFound)
	{
		Printf("Self-test FAILED: Desynchronization detected between client and server.\n");
	}
	else
	{
		Printf("Self-test PASSED: All states successfully synchronized with zero desyncs!\n");
	}
#else
	Printf("Self-test is only supported on Windows platform currently.\n");
#endif
}
