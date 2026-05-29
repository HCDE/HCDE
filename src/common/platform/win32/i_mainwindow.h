/*
** i_mainwindow.h
**
**
**
**---------------------------------------------------------------------------
**
** Copyright 1998-2016 Marisa Heit
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
** Code written prior to 2026 is also licensed under:
**
** SPDX-License-Identifier: BSD-3-Clause
**
**---------------------------------------------------------------------------
**
*/

#pragma once

#include "zstring.h"
#include "printf.h"

#include <functional>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// The WndProc used when the game view is active
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

class MainWindow
{
public:
	void Create(const FString& title, int x, int y, int width, int height);

	void ShowGameView();
	void RestoreConView();

	void ShowErrorPane(const char* text);
	bool CheckForRestart();

	void PrintStr(const char* cp);
	void GetLog(std::function<bool(const void* data, uint32_t size, uint32_t& written)> writeFile);

	void SetWindowTitle(const char* caption);

	HWND GetHandle() { return Window; }

#ifdef HCDE_DEDICATED_SERVER
	void TickServerConsole();
	void PumpServerConsoleStdIn();
	void SetServerConsoleStatus(const char* status);
#endif

private:
	static LRESULT CALLBACK LConProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifdef HCDE_DEDICATED_SERVER
	void CreateServerConsoleControls();
	void ResizeServerConsoleControls(int width, int height);
	static LRESULT CALLBACK ServerCommandProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
	void AppendServerConsoleText(const char* cp);
	void PumpServerConsoleMessages();
	void SubmitServerConsoleCommand();
	void RememberServerConsoleCommand(const FString& command);
	void NavigateServerConsoleCommandHistory(int direction);
	void SubmitServerMapChange();
	void SubmitServerBroadcast();
	void SubmitServerKick();
	void RefreshServerKickList();
	void ApplyServerConsolePreset();
	void ApplyServerConsoleFlag(bool enabled);
	void ApplyServerConsoleSetting(int applyId);
	void ApplyServerConsoleAdvancedSetting();
	void SyncServerConsoleSettingsFromCVars(bool logResult);
	void UpdateServerConsoleAdvancedSettingDefault();
	void ClearServerConsoleLog();
	void CopyServerConsoleLog();
	void RequestServerConsoleStart();
	void RequestServerConsoleShutdown();
	void SendServerConsoleCommand(const char* command);
	void UpdateServerConsoleStatusFromLogLine(const char* text);
#endif

	HWND Window = 0;
	bool restartrequest = false;
	TArray<FString> bufferedConsoleStuff;
	std::mutex logMutex;

#ifdef HCDE_DEDICATED_SERVER
	HWND ServerStatus = 0;
	HWND ServerLog = 0;
	HWND ServerCommand = 0;
	TArray<FString> ServerCommandHistory;
	int ServerCommandHistoryIndex = -1;
	HWND ServerSessionTitle = 0;
	HWND ServerMapLabel = 0;
	HWND ServerMap = 0;
	HWND ServerBroadcastLabel = 0;
	HWND ServerBroadcast = 0;
	HWND ServerKickLabel = 0;
	HWND ServerKick = 0;
	TArray<int> ServerKickClients;
	TArray<FString> ServerKickClientLabels;
	HWND ServerPresetLabel = 0;
	HWND ServerPresetCombo = 0;
	HWND ServerFlagLabel = 0;
	HWND ServerFlagCombo = 0;
	HWND ServerAdvancedSettingLabel = 0;
	HWND ServerAdvancedSettingCombo = 0;
	HWND ServerAdvancedSettingValue = 0;
	HWND ServerAdvancedSettingValueCombo = 0;
	enum { ServerMaxSettingControls = 12 };
	HWND ServerSettingsTitle = 0;
	HWND ServerSettingLabels[ServerMaxSettingControls] = {};
	HWND ServerSettingInputs[ServerMaxSettingControls] = {};
	FString ServerSettingLastApplied[ServerMaxSettingControls] = {};
	bool ServerSettingLastAppliedValid[ServerMaxSettingControls] = {};
	int ServerSettingCount = 0;
	HWND ServerAdvancedControlsLabel = 0;
	HFONT ServerFont = 0;
	HBRUSH ServerBrush = 0;
	bool ServerPumpingMessages = false;
	HANDLE ServerStdIn = INVALID_HANDLE_VALUE;
	bool ServerStdInUnavailable = false;
	FString ServerStdInBuffer;
#endif
};

extern MainWindow mainwindow;

#ifdef HCDE_DEDICATED_SERVER
// Poll dedicated server stdin for scripted console commands.
void I_PumpDedicatedServerConsoleInput();
#endif
