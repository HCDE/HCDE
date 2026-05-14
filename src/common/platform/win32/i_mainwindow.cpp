/*
** i_mainwindow.cpp
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


#include <richedit.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <stdlib.h>
#include <string.h>

#include "c_cvars.h"
#include "c_dispatch.h"
#include "d_net.h"
#include "engineerrors.h"
#include "gstrings.h"
#include "i_input.h"
#include "i_mainwindow.h"
#include "i_net.h"
#include "palentry.h"
#include "resource.h"
#include "st_start.h"
#include "startupinfo.h"
#include "utf8.h"
#include "v_font.h"
#include "version.h"
#include "widgets/errorwindow.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

MainWindow mainwindow;

#ifdef HCDE_DEDICATED_SERVER
enum
{
	IDC_HCDE_SERVER_LOG = 31000,
	IDC_HCDE_SERVER_COMMAND,
	IDC_HCDE_SERVER_SEND,
	IDC_HCDE_SERVER_STOP,
	IDC_HCDE_SERVER_STATUS,
	IDC_HCDE_SERVER_START_GAME,
	IDC_HCDE_SERVER_CLEAR_LOG,
	IDC_HCDE_SERVER_MAP,
	IDC_HCDE_SERVER_MAP_CHANGE,
	IDC_HCDE_SERVER_MAP_RESTART,
	IDC_HCDE_SERVER_BROADCAST,
	IDC_HCDE_SERVER_BROADCAST_SEND,
	IDC_HCDE_SERVER_KICK,
	IDC_HCDE_SERVER_KICK_SEND,
	IDC_HCDE_SERVER_PRESET,
	IDC_HCDE_SERVER_PRESET_APPLY,
	IDC_HCDE_SERVER_FLAG,
	IDC_HCDE_SERVER_FLAG_ON,
	IDC_HCDE_SERVER_FLAG_OFF,
	IDC_HCDE_SERVER_REFRESH_CVARS,
	IDC_HCDE_SERVER_COPY_LOG = 31020,
	IDC_HCDE_SERVER_APPLY_ALL_SETTINGS,
	IDC_HCDE_SERVER_ADVANCED_SETTING,
	IDC_HCDE_SERVER_ADVANCED_VALUE,
	IDC_HCDE_SERVER_ADVANCED_VALUE_CHOICE,
	IDC_HCDE_SERVER_ADVANCED_APPLY,
	IDC_HCDE_SERVER_SETTING_BASE = 31100,
	IDC_HCDE_SERVER_SETTING_APPLY_BASE = 31200,
};

static constexpr int ServerLogLimit = 1024 * 1024;
static constexpr int ServerCommandHistoryLimit = 64;

#define SERVER_GUI_ARRAY_COUNT(array) static_cast<int>(sizeof(array) / sizeof((array)[0]))

enum class ServerGuiSettingKind
{
	Text,
	Integer,
	Decimal,
	Choice
};

struct ServerGuiChoiceDefinition
{
	const wchar_t* Label;
	const char* Value;
};

struct ServerGuiSettingDefinition
{
	const wchar_t* Label;
	const char* CVarName;
	ServerGuiSettingKind Kind;
	const wchar_t* DefaultValue;
	double MinValue;
	double MaxValue;
	bool AllowEmpty;
	int TextLimit;
	const ServerGuiChoiceDefinition* Choices = nullptr;
	int ChoiceCount = 0;
};

static const ServerGuiChoiceDefinition ServerGuiGameTypeChoices[] =
{
	{ L"0 Co-op", "0" },
	{ L"1 Deathmatch", "1" },
	{ L"2 Team DM", "2" },
	{ L"3 CTF stub", "3" },
	{ L"4 Horde stub", "4" },
};

static const ServerGuiChoiceDefinition ServerGuiSkillChoices[] =
{
	{ L"0 ITYTD", "0" },
	{ L"1 HNTR", "1" },
	{ L"2 HMP", "2" },
	{ L"3 UV", "3" },
	{ L"4 Nightmare", "4" },
};

static const ServerGuiChoiceDefinition ServerGuiPauseChoices[] =
{
	{ L"0 Everyone", "0" },
	{ L"1 Controllers", "1" },
	{ L"2 Disabled", "2" },
};

static const ServerGuiChoiceDefinition ServerGuiReadyChoices[] =
{
	{ L"0 Vote", "0" },
	{ L"1 Anyone", "1" },
	{ L"2 Host only", "2" },
};

static const ServerGuiChoiceDefinition ServerGuiBoolChoices[] =
{
	{ L"0 Off", "0" },
	{ L"1 On", "1" },
};

static const ServerGuiChoiceDefinition ServerGuiFastWeaponChoices[] =
{
	{ L"0 Normal", "0" },
	{ L"1 Fast", "1" },
	{ L"2 Weapon only", "2" },
	{ L"3 Instant", "3" },
};

static const ServerGuiChoiceDefinition ServerGuiTallyChoices[] =
{
	{ L"0 Map rule", "0" },
	{ L"1 Allow tally", "1" },
	{ L"2 Force tally", "2" },
};

static const ServerGuiChoiceDefinition ServerGuiCompatModeChoices[] =
{
	{ L"0 Default", "0" },
	{ L"1 Doom relaxed", "1" },
	{ L"2 Doom strict", "2" },
	{ L"3 Boom", "3" },
	{ L"4 Old ZDoom", "4" },
	{ L"5 MBF", "5" },
	{ L"6 Boom strict", "6" },
	{ L"7 MBF strict", "7" },
	{ L"8 MBF21", "8" },
	{ L"9 MBF21 strict", "9" },
};

static const ServerGuiSettingDefinition ServerGuiSettings[] =
{
	{ L"Hostname", "sv_hostname", ServerGuiSettingKind::Text, L"Untitled HCDE Server", 0, 0, false, 96 },
	{ L"MOTD", "sv_motd", ServerGuiSettingKind::Text, L"Welcome to HCDE", 0, 0, true, 160 },
	{ L"Max Players", "sv_maxplayers", ServerGuiSettingKind::Integer, L"0", 0, 128, false, 3 },
	{ L"Game Type", "sv_gametype", ServerGuiSettingKind::Choice, L"0", 0, 4, false, 1, ServerGuiGameTypeChoices, SERVER_GUI_ARRAY_COUNT(ServerGuiGameTypeChoices) },
	{ L"Skill", "skill", ServerGuiSettingKind::Choice, L"2", 0, 4, false, 1, ServerGuiSkillChoices, SERVER_GUI_ARRAY_COUNT(ServerGuiSkillChoices) },
	{ L"Frag Limit", "fraglimit", ServerGuiSettingKind::Integer, L"0", 0, 999, false, 3 },
	{ L"Time Limit", "timelimit", ServerGuiSettingKind::Decimal, L"0", 0, 1440, false, 8 },
	{ L"NAT Port", "sv_natport", ServerGuiSettingKind::Integer, L"0", 0, 65535, false, 5 },
	{ L"Team Damage", "teamdamage", ServerGuiSettingKind::Decimal, L"0", 0, 100, false, 8 },
	{ L"Pause Policy", "net_disablepause", ServerGuiSettingKind::Choice, L"0", 0, 2, false, 1, ServerGuiPauseChoices, SERVER_GUI_ARRAY_COUNT(ServerGuiPauseChoices) },
	{ L"Ready Mode", "net_cutscenereadytype", ServerGuiSettingKind::Choice, L"0", 0, 2, false, 1, ServerGuiReadyChoices, SERVER_GUI_ARRAY_COUNT(ServerGuiReadyChoices) },
	{ L"Ready Time", "net_cutscenecountdown", ServerGuiSettingKind::Decimal, L"30", 0, 600, false, 8 },
};

static constexpr int ServerGuiSettingCount = sizeof(ServerGuiSettings) / sizeof(ServerGuiSettings[0]);

// Only expose advanced entries that are backed by HCDE runtime CVars today.
static const ServerGuiSettingDefinition ServerGuiAdvancedSettings[] =
{
	{ L"Raw DM Flags", "dmflags", ServerGuiSettingKind::Integer, L"0", 0, 2147483647, false, 10 },
	{ L"Raw DM Flags 2", "dmflags2", ServerGuiSettingKind::Integer, L"0", 0, 2147483647, false, 10 },
	{ L"Raw DM Flags 3", "dmflags3", ServerGuiSettingKind::Integer, L"0", 0, 2147483647, false, 10 },
	{ L"Gravity", "sv_gravity", ServerGuiSettingKind::Decimal, L"800", 0, 10000, false, 10 },
	{ L"Air Control", "sv_aircontrol", ServerGuiSettingKind::Decimal, L"0.00390625", 0, 1, false, 12 },
	{ L"Corpse Queue", "sv_corpsequeuesize", ServerGuiSettingKind::Integer, L"64", 0, 4096, false, 4 },
	{ L"Fast Weapons", "sv_fastweapons", ServerGuiSettingKind::Choice, L"0", 0, 3, false, 1, ServerGuiFastWeaponChoices, SERVER_GUI_ARRAY_COUNT(ServerGuiFastWeaponChoices) },
	{ L"Smart Aim", "sv_smartaim", ServerGuiSettingKind::Integer, L"0", 0, 2, false, 1 },
	{ L"Drop Style", "sv_dropstyle", ServerGuiSettingKind::Integer, L"0", 0, 4, false, 1 },
	{ L"Compat Mode", "compatmode", ServerGuiSettingKind::Choice, L"0", 0, 9, false, 1, ServerGuiCompatModeChoices, SERVER_GUI_ARRAY_COUNT(ServerGuiCompatModeChoices) },
	{ L"Raw Compat Flags", "compatflags", ServerGuiSettingKind::Integer, L"0", 0, 2147483647, false, 10 },
	{ L"Raw Compat Flags 2", "compatflags2", ServerGuiSettingKind::Integer, L"0", 0, 2147483647, false, 10 },
	{ L"Ready Percent", "net_cutscenereadypercent", ServerGuiSettingKind::Decimal, L"0.5", 0, 1, false, 8 },
	{ L"Chat Slowmode", "net_chatslowmode", ServerGuiSettingKind::Integer, L"0", 0, 3600, false, 4 },
	{ L"Tally Policy", "sv_alwaystally", ServerGuiSettingKind::Choice, L"0", 0, 2, false, 1, ServerGuiTallyChoices, SERVER_GUI_ARRAY_COUNT(ServerGuiTallyChoices) },
	{ L"Apply DM Flags Always", "alwaysapplydmflags", ServerGuiSettingKind::Choice, L"0", 0, 1, false, 1, ServerGuiBoolChoices, SERVER_GUI_ARRAY_COUNT(ServerGuiBoolChoices) },
};

static constexpr int ServerGuiAdvancedSettingCount = sizeof(ServerGuiAdvancedSettings) / sizeof(ServerGuiAdvancedSettings[0]);

struct ServerGuiPresetDefinition
{
	const wchar_t* Label;
	const char* LogName;
	const char* const* Commands;
	int CommandCount;
};

static const char* const ServerGuiPresetModCoop[] =
{
	"sv_gametype 0",
	"skill 2",
	"fraglimit 0",
	"timelimit 0",
	"sv_nomonsters 0",
	"sv_monsterrespawn 0",
	"sv_fastmonsters 0",
	"sv_itemrespawn 0",
	"sv_weaponstay 1",
	"sv_coopsharekeys 1",
	"sv_noweaponspawn 1",
	"sv_noplayerclip 1",
	"sv_localitems 1",
	"sv_rememberlastweapon 1",
	"sv_nojump 0",
	"sv_allowjump 1",
	"sv_nocrouch 0",
	"sv_allowcrouch 1"
};

static const char* const ServerGuiPresetSurvivalCoop[] =
{
	"sv_gametype 0",
	"skill 3",
	"fraglimit 0",
	"timelimit 0",
	"sv_nomonsters 0",
	"sv_monsterrespawn 1",
	"sv_fastmonsters 0",
	"sv_itemrespawn 1",
	"sv_weaponstay 1",
	"sv_coopsharekeys 1",
	"sv_localitems 1",
	"sv_rememberlastweapon 1",
	"sv_pistolstart 1"
};

static const char* const ServerGuiPresetClassicDeathmatch[] =
{
	"sv_gametype 1",
	"skill 2",
	"fraglimit 25",
	"timelimit 10",
	"sv_nomonsters 1",
	"sv_itemrespawn 0",
	"sv_weaponstay 1",
	"sv_spawnfarthest 1",
	"sv_forcerespawn 1"
};

static const char* const ServerGuiPresetAltDeathmatch[] =
{
	"sv_gametype 1",
	"skill 2",
	"fraglimit 25",
	"timelimit 10",
	"sv_nomonsters 1",
	"sv_itemrespawn 1",
	"sv_weaponstay 0",
	"sv_spawnfarthest 1",
	"sv_forcerespawn 1"
};

static const char* const ServerGuiPresetTeamDeathmatch[] =
{
	"sv_gametype 2",
	"skill 2",
	"fraglimit 50",
	"timelimit 15",
	"sv_nomonsters 1",
	"sv_itemrespawn 1",
	"sv_weaponstay 1",
	"sv_spawnfarthest 1",
	"sv_forcerespawn 1"
};

static const char* const ServerGuiPresetPrivateDebug[] =
{
	"sv_gametype 0",
	"skill 1",
	"fraglimit 0",
	"timelimit 0",
	"unadvertise",
	"sv_cheats 1",
	"debugtrace_enable 1"
};

static const ServerGuiPresetDefinition ServerGuiPresets[] =
{
	{ L"Mod-Friendly Co-op", "Mod-Friendly Co-op", ServerGuiPresetModCoop, sizeof(ServerGuiPresetModCoop) / sizeof(ServerGuiPresetModCoop[0]) },
	{ L"Survival Co-op", "Survival Co-op", ServerGuiPresetSurvivalCoop, sizeof(ServerGuiPresetSurvivalCoop) / sizeof(ServerGuiPresetSurvivalCoop[0]) },
	{ L"Classic Deathmatch", "Classic Deathmatch", ServerGuiPresetClassicDeathmatch, sizeof(ServerGuiPresetClassicDeathmatch) / sizeof(ServerGuiPresetClassicDeathmatch[0]) },
	{ L"Alt Deathmatch", "Alt Deathmatch", ServerGuiPresetAltDeathmatch, sizeof(ServerGuiPresetAltDeathmatch) / sizeof(ServerGuiPresetAltDeathmatch[0]) },
	{ L"Team Deathmatch", "Team Deathmatch", ServerGuiPresetTeamDeathmatch, sizeof(ServerGuiPresetTeamDeathmatch) / sizeof(ServerGuiPresetTeamDeathmatch[0]) },
	{ L"Private Debug", "Private Debug", ServerGuiPresetPrivateDebug, sizeof(ServerGuiPresetPrivateDebug) / sizeof(ServerGuiPresetPrivateDebug[0]) },
};

static constexpr int ServerGuiPresetCount = sizeof(ServerGuiPresets) / sizeof(ServerGuiPresets[0]);

struct ServerGuiFlagDefinition
{
	const wchar_t* Label;
	const char* LogName;
	const char* CVarName;
};

static const ServerGuiFlagDefinition ServerGuiFlags[] =
{
	{ L"Movement: Force Allow Jump", "Force Allow Jump", "sv_allowjump" },
	{ L"Movement: Force Disable Jump", "Force Disable Jump", "sv_nojump" },
	{ L"Movement: Force Allow Crouch", "Force Allow Crouch", "sv_allowcrouch" },
	{ L"Movement: Force Disable Crouch", "Force Disable Crouch", "sv_nocrouch" },
	{ L"Movement: Force Allow Freelook", "Force Allow Freelook", "sv_allowfreelook" },
	{ L"Movement: Force Disable Freelook", "Force Disable Freelook", "sv_nofreelook" },
	{ L"Movement: Disable FOV Changes", "Disable FOV Changes", "sv_nofov" },
	{ L"World: No Monsters", "No Monsters", "sv_nomonsters" },
	{ L"World: Respawn Monsters", "Respawn Monsters", "sv_monsterrespawn" },
	{ L"World: Fast Monsters", "Fast Monsters", "sv_fastmonsters" },
	{ L"World: Instant Monster Reaction", "Instant Monster Reaction", "sv_instantreaction" },
	{ L"World: Auto Compatibility", "Auto Compatibility", "sv_autocompat" },
	{ L"World: Kill All Monsters On Exit", "Kill All Monsters On Exit", "sv_killallmonsters" },
	{ L"Items: No Health", "No Health", "sv_nohealth" },
	{ L"Items: No Items", "No Items", "sv_noitems" },
	{ L"Items: No Armor", "No Armor", "sv_noarmor" },
	{ L"Items: Respawn Items", "Respawn Items", "sv_itemrespawn" },
	{ L"Items: Respawn Super Items", "Respawn Super Items", "sv_respawnsuper" },
	{ L"Items: Weapon Stay", "Weapon Stay", "sv_weaponstay" },
	{ L"Items: Weapon Drop", "Weapon Drop", "sv_weapondrop" },
	{ L"Items: Infinite Ammo", "Infinite Ammo", "sv_infiniteammo" },
	{ L"Items: Infinite Inventory", "Infinite Inventory", "sv_infiniteinventory" },
	{ L"Items: Double Ammo", "Double Ammo", "sv_doubleammo" },
	{ L"Co-op: Share Keys", "Share Co-op Keys", "sv_coopsharekeys" },
	{ L"Co-op: Local Items", "Local Items", "sv_localitems" },
	{ L"Co-op: No Local Drops", "No Local Drops", "sv_nolocaldrops" },
	{ L"Co-op: Pistol Start", "Pistol Start", "sv_pistolstart" },
	{ L"Co-op: Remember Last Weapon", "Remember Last Weapon", "sv_rememberlastweapon" },
	{ L"Co-op: No Multiplayer Weapons", "No Multiplayer Weapons", "sv_noweaponspawn" },
	{ L"Co-op: No Co-op Items", "No Co-op Items", "sv_nocoopitems" },
	{ L"Co-op: No Co-op Things", "No Co-op Things", "sv_nocoopthings" },
	{ L"Co-op: Lose Inventory", "Lose Inventory", "sv_cooploseinventory" },
	{ L"Co-op: Lose Keys", "Lose Keys", "sv_cooplosekeys" },
	{ L"Co-op: Lose Weapons", "Lose Weapons", "sv_cooploseweapons" },
	{ L"Co-op: Lose Armor", "Lose Armor", "sv_cooplosearmor" },
	{ L"Co-op: Lose Powerups", "Lose Powerups", "sv_cooplosepowerups" },
	{ L"Co-op: Lose Ammo", "Lose Ammo", "sv_cooploseammo" },
	{ L"Co-op: Halve Ammo", "Halve Ammo", "sv_coophalveammo" },
	{ L"DM: Same Level", "Same Level", "sv_samelevel" },
	{ L"DM: Spawn Farthest", "Spawn Farthest", "sv_spawnfarthest" },
	{ L"DM: Force Respawn", "Force Respawn", "sv_forcerespawn" },
	{ L"DM: No Exit", "No Exit", "sv_noexit" },
	{ L"DM: Keep Frags", "Keep Frags", "sv_keepfrags" },
	{ L"DM: No Respawn", "No Respawn", "sv_norespawn" },
	{ L"DM: Lose Frag On Death", "Lose Frag On Death", "sv_losefrag" },
	{ L"DM: Respawn Protection", "Respawn Protection", "sv_respawnprotect" },
	{ L"DM: Same Spawn Spot", "Same Spawn Spot", "sv_samespawnspot" },
	{ L"DM: No BFG Freeaim", "No BFG Freeaim", "sv_nobfgaim" },
	{ L"DM: Barrel Respawn", "Barrel Respawn", "sv_barrelrespawn" },
	{ L"DM: Disable Spying", "Disable Spying", "sv_disallowspying" },
	{ L"DM: Chasecam", "Chasecam", "sv_chasecam" },
	{ L"DM: Disable Suicide", "Disable Suicide", "sv_disallowsuicide" },
	{ L"DM: No Autoaim", "No Autoaim", "sv_noautoaim" },
	{ L"DM: Don't Check Ammo", "Don't Check Ammo", "sv_dontcheckammo" },
	{ L"DM: No Vertical Spread", "No Vertical Spread", "sv_novertspread" },
	{ L"DM: No Extra Ammo", "No Extra Ammo", "sv_noextraammo" },
	{ L"Team: No Team Switch", "No Team Switch", "sv_noteamswitch" },
	{ L"Map: No Automap", "No Automap", "sv_noautomap" },
	{ L"Map: No Allied Automap", "No Allied Automap", "sv_noautomapallies" },
	{ L"Compat: Allow All ACS Scripts", "Allow All ACS Scripts", "sv_allowallscripts" },
	{ L"Compat: Disable Auto Health", "Disable Auto Health", "sv_disableautohealth" },
	{ L"Admin: Cheats", "Cheats", "sv_cheats" },
	{ L"Network: Extra Net Tic", "Extra Net Tic", "net_extratic" },
	{ L"Network: Master Advertising", "Master Advertising", "sv_usemasters" },
	{ L"Network: UPnP", "UPnP", "sv_upnp" },
};

static constexpr int ServerGuiFlagCount = sizeof(ServerGuiFlags) / sizeof(ServerGuiFlags[0]);

static std::wstring ServerGuiWideString(const char* text)
{
	return WideString(text ? text : "");
}

static FString ServerGuiUtf8FromWide(const wchar_t* text)
{
	if (text == nullptr || *text == 0)
	{
		return FString();
	}

	int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
	if (bytes <= 1)
	{
		return FString();
	}

	TArray<char> utf8(bytes, true);
	WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.Data(), bytes, nullptr, nullptr);
	return FString(utf8.Data());
}

static bool ServerGuiGetCVarString(const char* cvarName, FString& out)
{
	FBaseCVar* cvar = FindCVar(cvarName, nullptr);
	if (cvar == nullptr)
	{
		return false;
	}

	out = cvar->GetGenericRep(CVAR_String).String;
	return true;
}

static bool ServerGuiGetCVarWideString(const char* cvarName, std::wstring& out)
{
	FString value;
	if (!ServerGuiGetCVarString(cvarName, value))
	{
		return false;
	}

	out = ServerGuiWideString(value.GetChars());
	return true;
}

static bool ServerGuiParseBoolChoiceValue(const char* value, int& out)
{
	if (value == nullptr || *value == 0)
	{
		return false;
	}
	if (stricmp(value, "true") == 0 || stricmp(value, "on") == 0 || stricmp(value, "yes") == 0)
	{
		out = 1;
		return true;
	}
	if (stricmp(value, "false") == 0 || stricmp(value, "off") == 0 || stricmp(value, "no") == 0)
	{
		out = 0;
		return true;
	}

	char* end = nullptr;
	long parsed = strtol(value, &end, 10);
	if (end != value && *end == 0 && (parsed == 0 || parsed == 1))
	{
		out = static_cast<int>(parsed);
		return true;
	}
	return false;
}

static bool ServerGuiChoiceValueMatches(const char* expected, const char* actual)
{
	if (expected == nullptr || actual == nullptr)
	{
		return false;
	}
	if (strcmp(expected, actual) == 0)
	{
		return true;
	}

	int expectedBool = 0;
	int actualBool = 0;
	return ServerGuiParseBoolChoiceValue(expected, expectedBool)
		&& ServerGuiParseBoolChoiceValue(actual, actualBool)
		&& expectedBool == actualBool;
}

static int ServerGuiFindChoiceByValue(const ServerGuiSettingDefinition& setting, const char* value)
{
	if (setting.Choices == nullptr || setting.ChoiceCount <= 0 || value == nullptr)
	{
		return -1;
	}

	for (int i = 0; i < setting.ChoiceCount; ++i)
	{
		if (ServerGuiChoiceValueMatches(setting.Choices[i].Value, value))
		{
			return i;
		}
	}
	return -1;
}

static void ServerGuiPopulateChoiceControl(HWND combo, const ServerGuiSettingDefinition& setting)
{
	if (combo == 0 || setting.Kind != ServerGuiSettingKind::Choice)
	{
		return;
	}

	SendMessage(combo, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < setting.ChoiceCount; ++i)
	{
		SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(setting.Choices[i].Label));
	}

	FString defaultValue = ServerGuiUtf8FromWide(setting.DefaultValue);
	int selected = ServerGuiFindChoiceByValue(setting, defaultValue.GetChars());
	SendMessage(combo, CB_SETCURSEL, selected >= 0 ? selected : 0, 0);
}

static void ServerGuiSetControlFromCVar(HWND control, const ServerGuiSettingDefinition& setting)
{
	if (control == 0)
	{
		return;
	}

	if (setting.Kind == ServerGuiSettingKind::Choice)
	{
		FString value = ServerGuiUtf8FromWide(setting.DefaultValue);
		FString cvarText;
		if (ServerGuiGetCVarString(setting.CVarName, cvarText))
		{
			cvarText.StripLeftRight();
			if (!cvarText.IsEmpty())
			{
				value = cvarText;
			}
		}

		int selected = ServerGuiFindChoiceByValue(setting, value.GetChars());
		if (selected < 0)
		{
			FString defaultValue = ServerGuiUtf8FromWide(setting.DefaultValue);
			selected = ServerGuiFindChoiceByValue(setting, defaultValue.GetChars());
		}
		SendMessage(control, CB_SETCURSEL, selected >= 0 ? selected : 0, 0);
		return;
	}

	std::wstring text = setting.DefaultValue;
	std::wstring cvarText;
	if (ServerGuiGetCVarWideString(setting.CVarName, cvarText))
	{
		text = cvarText;
	}

	SetWindowTextW(control, text.c_str());
	SendMessage(control, EM_SETLIMITTEXT, setting.TextLimit, 0);
}

static FString ServerGuiUtf8FromWindow(HWND window)
{
	int length = GetWindowTextLengthW(window);
	if (length <= 0)
	{
		return FString();
	}

	TArray<wchar_t> wide(length + 1, true);
	GetWindowTextW(window, wide.Data(), length + 1);

	int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.Data(), -1, nullptr, 0, nullptr, nullptr);
	if (bytes <= 1)
	{
		return FString();
	}

	TArray<char> utf8(bytes, true);
	WideCharToMultiByte(CP_UTF8, 0, wide.Data(), -1, utf8.Data(), bytes, nullptr, nullptr);
	return FString(utf8.Data());
}

static FString ServerGuiStripLogColors(const char* text)
{
	FString clean;
	if (text == nullptr)
	{
		return clean;
	}

	for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != 0; ++cursor)
	{
		if (*cursor >= 0x1c && *cursor <= 0x1f)
		{
			if (*cursor == 0x1c && cursor[1] != 0)
			{
				++cursor;
			}
			continue;
		}
		clean += static_cast<char>(*cursor);
	}
	return clean;
}

static FString ServerGuiQuotedConsoleString(const char* text)
{
	FString quoted = "\"";
	if (text != nullptr)
	{
		for (const char* cursor = text; *cursor != 0; ++cursor)
		{
			if (*cursor == '\\' || *cursor == '"')
			{
				quoted += '\\';
				quoted += *cursor;
			}
			else if (*cursor == ';' || *cursor == '\r' || *cursor == '\n' || *cursor == '\t')
			{
				quoted += ' ';
			}
			else
			{
				quoted += *cursor;
			}
		}
	}
	quoted += "\"";
	return quoted;
}

static bool ServerGuiIsSafeCommandToken(const FString& text)
{
	if (text.IsEmpty())
	{
		return false;
	}

	for (const char* cursor = text.GetChars(); *cursor != 0; ++cursor)
	{
		unsigned char ch = static_cast<unsigned char>(*cursor);
		if (ch <= ' ' || ch == '"' || ch == '\'' || ch == ';' || ch == '\\')
		{
			return false;
		}
	}
	return true;
}

static int ServerGuiSelectedComboIndex(HWND combo)
{
	LRESULT result = SendMessage(combo, CB_GETCURSEL, 0, 0);
	return result == CB_ERR ? -1 : static_cast<int>(result);
}

static FString ServerGuiValueFromControl(HWND control, const ServerGuiSettingDefinition& setting)
{
	if (setting.Kind == ServerGuiSettingKind::Choice)
	{
		int index = ServerGuiSelectedComboIndex(control);
		if (index >= 0 && index < setting.ChoiceCount)
		{
			return FString(setting.Choices[index].Value);
		}
		return FString();
	}

	return ServerGuiUtf8FromWindow(control);
}

static bool ServerGuiBuildSettingCommand(const ServerGuiSettingDefinition& setting, const FString& rawValue, FString& command, FString& warning)
{
	FString value = rawValue;
	value.StripLeftRight();
	if (value.IsEmpty() && !setting.AllowEmpty)
	{
		warning.Format("HCDE server console: enter a value for %s before applying it.\n", setting.CVarName);
		return false;
	}

	if (setting.Kind == ServerGuiSettingKind::Text)
	{
		command.Format("%s %s", setting.CVarName, ServerGuiQuotedConsoleString(value.GetChars()).GetChars());
		return true;
	}

	if (setting.Kind == ServerGuiSettingKind::Choice)
	{
		int choice = ServerGuiFindChoiceByValue(setting, value.GetChars());
		if (choice < 0)
		{
			warning.Format("HCDE server console: choose a valid option for %s before applying it.\n", setting.CVarName);
			return false;
		}
		command.Format("%s %s", setting.CVarName, setting.Choices[choice].Value);
		return true;
	}

	if (setting.Kind == ServerGuiSettingKind::Integer)
	{
		char* end = nullptr;
		long parsed = strtol(value.GetChars(), &end, 10);
		if (end == value.GetChars() || *end != 0 || parsed < setting.MinValue || parsed > setting.MaxValue)
		{
			warning.Format("HCDE server console: %s must be between %d and %d.\n",
				setting.CVarName, static_cast<int>(setting.MinValue), static_cast<int>(setting.MaxValue));
			return false;
		}
		command.Format("%s %ld", setting.CVarName, parsed);
		return true;
	}

	char* end = nullptr;
	double parsed = strtod(value.GetChars(), &end);
	if (end == value.GetChars() || *end != 0 || parsed < setting.MinValue || parsed > setting.MaxValue)
	{
		warning.Format("HCDE server console: %s must be between %.6g and %.6g.\n",
			setting.CVarName, setting.MinValue, setting.MaxValue);
		return false;
	}
	command.Format("%s %.8g", setting.CVarName, parsed);
	return true;
}

static HWND CreateServerChild(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int id)
{
	return CreateWindowExW(
		0,
		cls,
		text,
		WS_CHILD | WS_VISIBLE | style,
		0, 0, 10, 10,
		parent,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
		GetModuleHandle(0),
		nullptr);
}
#endif

void MainWindow::Create(const FString& caption, int x, int y, int width, int height)
{
	static const WCHAR WinClassName[] = L"MainWindow";

	HINSTANCE hInstance = GetModuleHandle(0);

#ifdef HCDE_DEDICATED_SERVER
	width = 980;
	height = 720;
	RECT workArea = {};
	if (SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0))
	{
		x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
		y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
	}
#endif

	WNDCLASS WndClass;
	WndClass.style = 0;
	WndClass.lpfnWndProc = LConProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = WinClassName;

	/* register this new class with Windows */
	if (!RegisterClass((LPWNDCLASS)&WndClass))
	{
		MessageBoxA(nullptr, "Could not register window class", "Fatal", MB_ICONEXCLAMATION | MB_OK);
		exit(-1);
	}

	std::wstring wcaption = caption.WideString();
	Window = CreateWindowExW(
		WS_EX_APPWINDOW,
		WinClassName,
		wcaption.c_str(),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		x, y, width, height,
		(HWND)NULL,
		(HMENU)NULL,
		hInstance,
		NULL);

	if (!Window)
	{
		MessageBoxA(nullptr, "Unable to create main window", "Fatal", MB_ICONEXCLAMATION | MB_OK);
		exit(-1);
	}

	uint32_t bordercolor = RGB(51, 51, 51);
	uint32_t captioncolor = RGB(33, 33, 33);
	uint32_t textcolor = RGB(226, 223, 219);

	// Don't error check these as they only exist on Windows 11, and if they fail then that is OK.
	DwmSetWindowAttribute(Window, 34/*DWMWA_BORDER_COLOR*/, &bordercolor, sizeof(uint32_t));
	DwmSetWindowAttribute(Window, 35/*DWMWA_CAPTION_COLOR*/, &captioncolor, sizeof(uint32_t));
	DwmSetWindowAttribute(Window, 36/*DWMWA_TEXT_COLOR*/, &textcolor, sizeof(uint32_t));

#ifdef HCDE_DEDICATED_SERVER
	CreateServerConsoleControls();
	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);
#endif
}

// Sets the main WndProc, hides all the child windows, and starts up in-game input.
void MainWindow::ShowGameView()
{
	if (GetWindowLongPtr(Window, GWLP_USERDATA) == 0)
	{
		SetWindowLongPtr(Window, GWLP_USERDATA, 1);
		SetWindowLongPtr(Window, GWLP_WNDPROC, (LONG_PTR)WndProc);
		I_InitInput(Window);
	}
}

// Returns the main window to its startup state.
void MainWindow::RestoreConView()
{
	I_ShutdownInput();		// Make sure the mouse pointer is available.
	ShowWindow(Window, SW_HIDE);

	// Make sure the progress bar isn't visible.
	DeleteStartupScreen();
}

// Shows an error message, preferably in the main window, but it can use a normal message box too.
void MainWindow::ShowErrorPane(const char* text)
{
	if (StartWindow)	// Ensure that the network pane is hidden.
	{
		I_NetDone();
	}

	// PrintStr(text);

	size_t totalsize = 0;
	for (const FString& line : bufferedConsoleStuff)
		totalsize += line.Len();

	std::string alltext;
	alltext.reserve(totalsize);
	for (const FString& line : bufferedConsoleStuff)
		alltext.append(line.GetChars(), line.Len());

	restartrequest = ErrorWindow::ExecModal(text, alltext);
}

bool MainWindow::CheckForRestart()
{
	bool result = restartrequest;
	restartrequest = false;
	return result;
}

// The main window's WndProc during startup. During gameplay, the WndProc in i_input.cpp is used instead.
LRESULT MainWindow::LConProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
#ifdef HCDE_DEDICATED_SERVER
	switch (msg)
	{
	case WM_GETMINMAXINFO:
	{
		MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
		info->ptMinTrackSize.x = 860;
		info->ptMinTrackSize.y = 560;
		return 0;
	}

	case WM_SIZE:
		mainwindow.ResizeServerConsoleControls(LOWORD(lParam), HIWORD(lParam));
		return 0;

	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED &&
			LOWORD(wParam) >= IDC_HCDE_SERVER_SETTING_APPLY_BASE &&
			LOWORD(wParam) < IDC_HCDE_SERVER_SETTING_APPLY_BASE + ServerGuiSettingCount)
		{
			mainwindow.ApplyServerConsoleSetting(LOWORD(wParam));
			return 0;
		}

		switch (LOWORD(wParam))
		{
		case IDC_HCDE_SERVER_SEND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.SubmitServerConsoleCommand();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_STOP:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.RequestServerConsoleShutdown();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_START_GAME:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.RequestServerConsoleStart();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_CLEAR_LOG:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.ClearServerConsoleLog();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_COPY_LOG:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.CopyServerConsoleLog();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_MAP_CHANGE:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.SubmitServerMapChange();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_MAP_RESTART:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.SendServerConsoleCommand("changemap *");
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_BROADCAST_SEND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.SubmitServerBroadcast();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_KICK_SEND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.SubmitServerKick();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_PRESET_APPLY:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.ApplyServerConsolePreset();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_FLAG_ON:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.ApplyServerConsoleFlag(true);
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_FLAG_OFF:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.ApplyServerConsoleFlag(false);
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_REFRESH_CVARS:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.SyncServerConsoleSettingsFromCVars(true);
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_APPLY_ALL_SETTINGS:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.ApplyServerConsoleVisibleSettings();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_ADVANCED_SETTING:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				mainwindow.UpdateServerConsoleAdvancedSettingDefault();
				return 0;
			}
			break;

		case IDC_HCDE_SERVER_ADVANCED_APPLY:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				mainwindow.ApplyServerConsoleAdvancedSetting();
				return 0;
			}
			break;
		}
		break;

	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORSTATIC:
		if (mainwindow.ServerBrush != 0)
		{
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, RGB(226, 223, 219));
			SetBkColor(hdc, RGB(18, 22, 24));
			return reinterpret_cast<LRESULT>(mainwindow.ServerBrush);
		}
		break;

	case WM_CLOSE:
		mainwindow.RequestServerConsoleShutdown();
		DestroyWindow(hWnd);
		return 0;

	case WM_DESTROY:
		if (mainwindow.ServerCommand != 0)
		{
			RemoveWindowSubclass(mainwindow.ServerCommand, ServerCommandProc, 1);
			mainwindow.ServerCommand = 0;
		}
		if (mainwindow.ServerFont != 0)
		{
			DeleteObject(mainwindow.ServerFont);
			mainwindow.ServerFont = 0;
		}
		if (mainwindow.ServerBrush != 0)
		{
			DeleteObject(mainwindow.ServerBrush);
			mainwindow.ServerBrush = 0;
		}
		PostQuitMessage(0);
		return 0;
	}
#endif
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void MainWindow::PrintStr(const char* cp)
{
	if (cp == nullptr)
	{
		cp = "";
	}
	bufferedConsoleStuff.Push(cp);
#ifdef HCDE_DEDICATED_SERVER
	while (bufferedConsoleStuff.Size() > 4096)
	{
		bufferedConsoleStuff.Delete(0);
	}
	AppendServerConsoleText(cp);
	PumpServerConsoleMessages();
#endif
}

void MainWindow::GetLog(std::function<bool(const void* data, uint32_t size, uint32_t& written)> writeData)
{
	for (const FString& line : bufferedConsoleStuff)
	{
		size_t pos = 0;
		size_t len = line.Len();
		while (pos < len)
		{
			uint32_t size = (uint32_t)std::min(len - pos, 0x0fffffffULL);
			uint32_t written = 0;
			if (!writeData(&line[pos], size, written))
				return;
			pos += written;
		}
	}
}

// each platform has its own specific version of this function.
void MainWindow::SetWindowTitle(const char* caption)
{
	std::wstring widecaption;
	if (!caption)
	{
		FStringf default_caption("" GAMENAME " %s (%s)", GetVersionString(), GetGitTime());
		widecaption = default_caption.WideString();
	}
	else
	{
		widecaption = WideString(caption);
	}
	SetWindowText(Window, widecaption.c_str());
}

#ifdef HCDE_DEDICATED_SERVER
void MainWindow::SetServerConsoleStatus(const char* status)
{
	if (ServerStatus == 0)
	{
		return;
	}

	FString clean = ServerGuiStripLogColors(status);
	clean.StripLeftRight();
	if (clean.IsEmpty())
	{
		clean = "idle";
	}

	FString text;
	text.Format("Status: %s", clean.GetChars());
	std::wstring wide = ServerGuiWideString(text.GetChars());
	SetWindowTextW(ServerStatus, wide.c_str());
}

LRESULT CALLBACK MainWindow::ServerCommandProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData)
{
	MainWindow* window = reinterpret_cast<MainWindow*>(refData);
	if (msg == WM_NCDESTROY)
	{
		RemoveWindowSubclass(hWnd, ServerCommandProc, subclassId);
		return DefSubclassProc(hWnd, msg, wParam, lParam);
	}

	if (window != nullptr && msg == WM_KEYDOWN)
	{
		switch (wParam)
		{
		case VK_RETURN:
			window->SubmitServerConsoleCommand();
			return 0;

		case VK_UP:
			window->NavigateServerConsoleCommandHistory(-1);
			return 0;

		case VK_DOWN:
			window->NavigateServerConsoleCommandHistory(1);
			return 0;

		case VK_ESCAPE:
			SetWindowTextW(hWnd, L"");
			window->ServerCommandHistoryIndex = -1;
			return 0;
		}
	}

	return DefSubclassProc(hWnd, msg, wParam, lParam);
}

void MainWindow::CreateServerConsoleControls()
{
	ServerBrush = CreateSolidBrush(RGB(18, 22, 24));
	ServerFont = CreateFontW(
		-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, FF_DONTCARE, L"Consolas");

	ServerStatus = CreateServerChild(Window, L"STATIC", L"Status: starting up", SS_LEFT, IDC_HCDE_SERVER_STATUS);
	ServerLog = CreateServerChild(Window, L"EDIT", L"",
		ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_BORDER,
		IDC_HCDE_SERVER_LOG);
	ServerCommand = CreateServerChild(Window, L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER, IDC_HCDE_SERVER_COMMAND);
	if (!SetWindowSubclass(ServerCommand, ServerCommandProc, 1, reinterpret_cast<DWORD_PTR>(this)))
	{
		PrintStr("HCDE server console: command history key binding install failed.\n");
	}

	HWND send = CreateServerChild(Window, L"BUTTON", L"Send", BS_PUSHBUTTON, IDC_HCDE_SERVER_SEND);
	HWND start = CreateServerChild(Window, L"BUTTON", L"Start Game", BS_PUSHBUTTON, IDC_HCDE_SERVER_START_GAME);
	HWND stop = CreateServerChild(Window, L"BUTTON", L"Stop Server", BS_PUSHBUTTON, IDC_HCDE_SERVER_STOP);
	HWND copyLog = CreateServerChild(Window, L"BUTTON", L"Copy Log", BS_PUSHBUTTON, IDC_HCDE_SERVER_COPY_LOG);

	ServerSessionTitle = CreateServerChild(Window, L"STATIC", L"Session Controls", SS_LEFT, 0);
	ServerMapLabel = CreateServerChild(Window, L"STATIC", L"Map", SS_LEFT, 0);
	ServerMap = CreateServerChild(Window, L"EDIT", L"MAP01", ES_AUTOHSCROLL | WS_BORDER, IDC_HCDE_SERVER_MAP);
	ServerBroadcastLabel = CreateServerChild(Window, L"STATIC", L"Broadcast Message", SS_LEFT, 0);
	ServerBroadcast = CreateServerChild(Window, L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER, IDC_HCDE_SERVER_BROADCAST);
	ServerKickLabel = CreateServerChild(Window, L"STATIC", L"Kick Player", SS_LEFT, 0);
	ServerKick = CreateServerChild(Window, L"COMBOBOX", L"",
		CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_HCDE_SERVER_KICK);
	HWND mapChange = CreateServerChild(Window, L"BUTTON", L"Change", BS_PUSHBUTTON, IDC_HCDE_SERVER_MAP_CHANGE);
	HWND mapRestart = CreateServerChild(Window, L"BUTTON", L"Restart", BS_PUSHBUTTON, IDC_HCDE_SERVER_MAP_RESTART);
	HWND broadcastSend = CreateServerChild(Window, L"BUTTON", L"Send", BS_PUSHBUTTON, IDC_HCDE_SERVER_BROADCAST_SEND);
	HWND kickSend = CreateServerChild(Window, L"BUTTON", L"Kick", BS_PUSHBUTTON, IDC_HCDE_SERVER_KICK_SEND);
	HWND clearLog = CreateServerChild(Window, L"BUTTON", L"Clear Log", BS_PUSHBUTTON, IDC_HCDE_SERVER_CLEAR_LOG);
	SendMessage(ServerMap, EM_SETLIMITTEXT, 32, 0);
	SendMessage(ServerBroadcast, EM_SETLIMITTEXT, 200, 0);
	SendMessageW(ServerCommand, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Type a server command"));
	SendMessageW(ServerMap, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"MAP01"));
	SendMessageW(ServerBroadcast, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Message to players"));

	ServerSettingsTitle = CreateServerChild(Window, L"STATIC", L"Server Settings", SS_LEFT, 0);
	HWND refreshCvars = CreateServerChild(Window, L"BUTTON", L"Refresh", BS_PUSHBUTTON, IDC_HCDE_SERVER_REFRESH_CVARS);
	HWND applyAllSettings = CreateServerChild(Window, L"BUTTON", L"Apply All", BS_PUSHBUTTON, IDC_HCDE_SERVER_APPLY_ALL_SETTINGS);
	ServerSettingCount = ServerGuiSettingCount;
	if (ServerSettingCount > ServerMaxSettingControls)
	{
		ServerSettingCount = ServerMaxSettingControls;
	}
	for (int i = 0; i < ServerSettingCount; ++i)
	{
		const ServerGuiSettingDefinition& setting = ServerGuiSettings[i];
		ServerSettingLabels[i] = CreateServerChild(Window, L"STATIC", setting.Label, SS_LEFT, 0);
		if (setting.Kind == ServerGuiSettingKind::Choice)
		{
			ServerSettingInputs[i] = CreateServerChild(Window, L"COMBOBOX", L"",
				CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_HCDE_SERVER_SETTING_BASE + i);
			ServerGuiPopulateChoiceControl(ServerSettingInputs[i], setting);
		}
		else
		{
			DWORD inputStyle = ES_AUTOHSCROLL | WS_BORDER;
			if (setting.Kind == ServerGuiSettingKind::Integer)
			{
				inputStyle |= ES_NUMBER;
			}
			ServerSettingInputs[i] = CreateServerChild(Window, L"EDIT", setting.DefaultValue, inputStyle, IDC_HCDE_SERVER_SETTING_BASE + i);
			SendMessage(ServerSettingInputs[i], EM_SETLIMITTEXT, setting.TextLimit, 0);
		}
		ServerSettingButtons[i] = CreateServerChild(Window, L"BUTTON", L"Apply", BS_PUSHBUTTON, IDC_HCDE_SERVER_SETTING_APPLY_BASE + i);
	}

	ServerAdvancedControlsLabel = CreateServerChild(Window, L"STATIC", L"Presets & Flags", SS_LEFT, 0);
	ServerPresetLabel = CreateServerChild(Window, L"STATIC", L"Preset", SS_LEFT, 0);
	ServerPresetCombo = CreateServerChild(Window, L"COMBOBOX", L"",
		CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_HCDE_SERVER_PRESET);
	HWND presetApply = CreateServerChild(Window, L"BUTTON", L"Apply", BS_PUSHBUTTON, IDC_HCDE_SERVER_PRESET_APPLY);
	for (int i = 0; i < ServerGuiPresetCount; ++i)
	{
		SendMessageW(ServerPresetCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ServerGuiPresets[i].Label));
	}
	SendMessage(ServerPresetCombo, CB_SETCURSEL, 0, 0);

	ServerFlagLabel = CreateServerChild(Window, L"STATIC", L"Flag", SS_LEFT, 0);
	ServerFlagCombo = CreateServerChild(Window, L"COMBOBOX", L"",
		CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_HCDE_SERVER_FLAG);
	HWND flagOn = CreateServerChild(Window, L"BUTTON", L"On", BS_PUSHBUTTON, IDC_HCDE_SERVER_FLAG_ON);
	HWND flagOff = CreateServerChild(Window, L"BUTTON", L"Off", BS_PUSHBUTTON, IDC_HCDE_SERVER_FLAG_OFF);
	for (int i = 0; i < ServerGuiFlagCount; ++i)
	{
		SendMessageW(ServerFlagCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ServerGuiFlags[i].Label));
	}
	SendMessage(ServerFlagCombo, CB_SETCURSEL, 0, 0);

	ServerAdvancedSettingLabel = CreateServerChild(Window, L"STATIC", L"Advanced CVar", SS_LEFT, 0);
	ServerAdvancedSettingCombo = CreateServerChild(Window, L"COMBOBOX", L"",
		CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_HCDE_SERVER_ADVANCED_SETTING);
	ServerAdvancedSettingValue = CreateServerChild(Window, L"EDIT", L"",
		ES_AUTOHSCROLL | WS_BORDER, IDC_HCDE_SERVER_ADVANCED_VALUE);
	ServerAdvancedSettingValueCombo = CreateServerChild(Window, L"COMBOBOX", L"",
		CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_HCDE_SERVER_ADVANCED_VALUE_CHOICE);
	HWND advancedApply = CreateServerChild(Window, L"BUTTON", L"Apply", BS_PUSHBUTTON, IDC_HCDE_SERVER_ADVANCED_APPLY);
	for (int i = 0; i < ServerGuiAdvancedSettingCount; ++i)
	{
		SendMessageW(ServerAdvancedSettingCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ServerGuiAdvancedSettings[i].Label));
	}
	SendMessage(ServerAdvancedSettingCombo, CB_SETCURSEL, 0, 0);
	SyncServerConsoleSettingsFromCVars(false);

	HWND controls[] =
	{
		ServerStatus, ServerLog, ServerCommand, send, start, stop,
		ServerSessionTitle, ServerMapLabel, ServerMap, mapChange, mapRestart,
		ServerBroadcastLabel, ServerBroadcast, broadcastSend,
		ServerKickLabel, ServerKick, kickSend, clearLog, copyLog,
		ServerSettingsTitle, refreshCvars, applyAllSettings, ServerAdvancedControlsLabel,
		ServerPresetLabel, ServerPresetCombo, presetApply,
		ServerFlagLabel, ServerFlagCombo, flagOn, flagOff,
		ServerAdvancedSettingLabel, ServerAdvancedSettingCombo, ServerAdvancedSettingValue, ServerAdvancedSettingValueCombo, advancedApply
	};
	for (HWND control : controls)
	{
		if (control != 0)
		{
			SendMessage(control, WM_SETFONT, reinterpret_cast<WPARAM>(ServerFont), TRUE);
		}
	}
	for (int i = 0; i < ServerSettingCount; ++i)
	{
		SendMessage(ServerSettingLabels[i], WM_SETFONT, reinterpret_cast<WPARAM>(ServerFont), TRUE);
		SendMessage(ServerSettingInputs[i], WM_SETFONT, reinterpret_cast<WPARAM>(ServerFont), TRUE);
		SendMessage(ServerSettingButtons[i], WM_SETFONT, reinterpret_cast<WPARAM>(ServerFont), TRUE);
	}

	SendMessage(ServerLog, EM_SETLIMITTEXT, ServerLogLimit, 0);
	RefreshServerKickList();

	RECT rect = {};
	GetClientRect(Window, &rect);
	ResizeServerConsoleControls(rect.right, rect.bottom);

	for (const FString& line : bufferedConsoleStuff)
	{
		AppendServerConsoleText(line.GetChars());
	}
}

void MainWindow::ResizeServerConsoleControls(int width, int height)
{
	if (ServerLog == 0)
	{
		return;
	}

	const int pad = 12;
	const int commandHeight = 28;
	const int buttonWidth = 72;
	const int topButtonWidth = 104;
	const int optionsWidth = 330;
	const int headerHeight = 30;
	const int logWidth = width - optionsWidth - (pad * 3);
	const int logHeight = height - headerHeight - commandHeight - (pad * 4);
	const int optionsX = pad + logWidth + pad;
	int y = pad;

	MoveWindow(ServerStatus, pad, y, width - (pad * 2) - (topButtonWidth * 2) - 16, 22, TRUE);
	HWND start = GetDlgItem(Window, IDC_HCDE_SERVER_START_GAME);
	HWND stop = GetDlgItem(Window, IDC_HCDE_SERVER_STOP);
	MoveWindow(start, width - pad - (topButtonWidth * 2) - 8, y - 2, topButtonWidth, 26, TRUE);
	MoveWindow(stop, width - pad - topButtonWidth, y - 2, topButtonWidth, 26, TRUE);

	y += headerHeight;
	MoveWindow(ServerLog, pad, y, logWidth, logHeight, TRUE);
	MoveWindow(ServerCommand, pad, height - pad - commandHeight, logWidth - buttonWidth - 8, commandHeight, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_SEND), pad + logWidth - buttonWidth, height - pad - commandHeight, buttonWidth, commandHeight, TRUE);

	int optionsY = y;
	MoveWindow(ServerSessionTitle, optionsX, optionsY, optionsWidth, 20, TRUE);
	optionsY += 25;
	MoveWindow(ServerMapLabel, optionsX, optionsY, optionsWidth, 18, TRUE);
	optionsY += 20;
	MoveWindow(ServerMap, optionsX, optionsY, optionsWidth - 176, 24, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_MAP_CHANGE), optionsX + optionsWidth - 168, optionsY, 78, 24, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_MAP_RESTART), optionsX + optionsWidth - 82, optionsY, 82, 24, TRUE);
	optionsY += 34;
	MoveWindow(ServerBroadcastLabel, optionsX, optionsY, optionsWidth, 18, TRUE);
	optionsY += 20;
	MoveWindow(ServerBroadcast, optionsX, optionsY, optionsWidth - 88, 24, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_BROADCAST_SEND), optionsX + optionsWidth - 80, optionsY, 80, 24, TRUE);
	optionsY += 34;
	MoveWindow(ServerKickLabel, optionsX, optionsY + 3, 78, 20, TRUE);
	MoveWindow(ServerKick, optionsX + 84, optionsY, optionsWidth - 154, 160, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_KICK_SEND), optionsX + optionsWidth - 62, optionsY, 62, 24, TRUE);
	optionsY += 30;
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_CLEAR_LOG), optionsX, optionsY, (optionsWidth / 2) - 4, 24, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_COPY_LOG), optionsX + (optionsWidth / 2) + 4, optionsY, (optionsWidth / 2) - 4, 24, TRUE);
	optionsY += 34;

	MoveWindow(ServerSettingsTitle, optionsX, optionsY, optionsWidth - 150, 22, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_APPLY_ALL_SETTINGS), optionsX + optionsWidth - 142, optionsY - 2, 76, 24, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_REFRESH_CVARS), optionsX + optionsWidth - 62, optionsY - 2, 62, 24, TRUE);
	optionsY += 26;
	for (int i = 0; i < ServerSettingCount; ++i)
	{
		const int inputHeight = ServerGuiSettings[i].Kind == ServerGuiSettingKind::Choice ? 140 : 22;
		MoveWindow(ServerSettingLabels[i], optionsX, optionsY + 3, 112, 20, TRUE);
		MoveWindow(ServerSettingInputs[i], optionsX + 118, optionsY, optionsWidth - 188, inputHeight, TRUE);
		MoveWindow(ServerSettingButtons[i], optionsX + optionsWidth - 62, optionsY, 62, 22, TRUE);
		optionsY += 25;
	}

	optionsY += 6;
	MoveWindow(ServerAdvancedControlsLabel, optionsX, optionsY, optionsWidth, 20, TRUE);
	optionsY += 22;
	MoveWindow(ServerPresetLabel, optionsX, optionsY + 3, 54, 20, TRUE);
	MoveWindow(ServerPresetCombo, optionsX + 58, optionsY, optionsWidth - 128, 160, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_PRESET_APPLY), optionsX + optionsWidth - 62, optionsY, 62, 24, TRUE);
	optionsY += 30;
	MoveWindow(ServerFlagLabel, optionsX, optionsY + 3, 42, 20, TRUE);
	MoveWindow(ServerFlagCombo, optionsX + 46, optionsY, optionsWidth - 136, 180, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_FLAG_ON), optionsX + optionsWidth - 82, optionsY, 36, 24, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_FLAG_OFF), optionsX + optionsWidth - 40, optionsY, 40, 24, TRUE);
	optionsY += 30;
	MoveWindow(ServerAdvancedSettingLabel, optionsX, optionsY + 3, 108, 20, TRUE);
	MoveWindow(ServerAdvancedSettingCombo, optionsX + 112, optionsY, optionsWidth - 112, 180, TRUE);
	optionsY += 30;
	MoveWindow(ServerAdvancedSettingValue, optionsX, optionsY, optionsWidth - 70, 24, TRUE);
	MoveWindow(ServerAdvancedSettingValueCombo, optionsX, optionsY, optionsWidth - 70, 180, TRUE);
	MoveWindow(GetDlgItem(Window, IDC_HCDE_SERVER_ADVANCED_APPLY), optionsX + optionsWidth - 62, optionsY, 62, 24, TRUE);
}

void MainWindow::AppendServerConsoleText(const char* cp)
{
	if (ServerLog == 0 || cp == nullptr || *cp == 0)
	{
		return;
	}

	FString clean = ServerGuiStripLogColors(cp);
	UpdateServerConsoleStatusFromLogLine(clean.GetChars());
	std::wstring wide = ServerGuiWideString(clean.GetChars());

	int currentLength = GetWindowTextLengthW(ServerLog);
	if (currentLength > ServerLogLimit)
	{
		SendMessage(ServerLog, EM_SETSEL, 0, currentLength - (ServerLogLimit / 2));
		SendMessageW(ServerLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L""));
	}

	SendMessage(ServerLog, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
	SendMessageW(ServerLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wide.c_str()));
	SendMessage(ServerLog, EM_SCROLLCARET, 0, 0);
}

void MainWindow::PumpServerConsoleMessages()
{
	if (Window == 0 || ServerPumpingMessages)
	{
		return;
	}

	ServerPumpingMessages = true;
	MSG msg = {};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			PostQuitMessage(static_cast<int>(msg.wParam));
			break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	ServerPumpingMessages = false;
}

void MainWindow::TickServerConsole()
{
	RefreshServerKickList();
	PumpServerConsoleMessages();
}

void I_PumpDedicatedServerConsoleWindow()
{
	mainwindow.TickServerConsole();
}

void MainWindow::SendServerConsoleCommand(const char* command)
{
	FString text(command ? command : "");
	text.StripLeftRight();
	if (text.IsEmpty())
	{
		return;
	}

	FString echo;
	echo.Format("> %s\n", text.GetChars());
	PrintStr(echo.GetChars());
	AddCommandString(text.GetChars());
}

void MainWindow::SubmitServerConsoleCommand()
{
	FString command = ServerGuiUtf8FromWindow(ServerCommand);
	command.StripLeftRight();
	if (command.IsEmpty())
	{
		return;
	}

	RememberServerConsoleCommand(command);
	SetWindowTextW(ServerCommand, L"");
	SendServerConsoleCommand(command.GetChars());
}

void MainWindow::RememberServerConsoleCommand(const FString& command)
{
	FString text = command;
	text.StripLeftRight();
	if (text.IsEmpty())
	{
		return;
	}

	const unsigned int count = ServerCommandHistory.Size();
	if (count > 0 && strcmp(ServerCommandHistory[count - 1].GetChars(), text.GetChars()) == 0)
	{
		ServerCommandHistoryIndex = -1;
		return;
	}

	ServerCommandHistory.Push(text);
	while (ServerCommandHistory.Size() > ServerCommandHistoryLimit)
	{
		ServerCommandHistory.Delete(0);
	}
	ServerCommandHistoryIndex = -1;
}

void MainWindow::NavigateServerConsoleCommandHistory(int direction)
{
	if (ServerCommand == 0 || ServerCommandHistory.Size() == 0)
	{
		return;
	}

	const int count = static_cast<int>(ServerCommandHistory.Size());
	if (ServerCommandHistoryIndex < 0)
	{
		if (direction >= 0)
		{
			return;
		}
		ServerCommandHistoryIndex = count - 1;
	}
	else
	{
		ServerCommandHistoryIndex += direction;
		if (ServerCommandHistoryIndex < 0)
		{
			ServerCommandHistoryIndex = 0;
		}
		else if (ServerCommandHistoryIndex >= count)
		{
			ServerCommandHistoryIndex = -1;
			SetWindowTextW(ServerCommand, L"");
			return;
		}
	}

	std::wstring command = ServerGuiWideString(ServerCommandHistory[ServerCommandHistoryIndex].GetChars());
	SetWindowTextW(ServerCommand, command.c_str());
	SendMessage(ServerCommand, EM_SETSEL, static_cast<WPARAM>(command.size()), static_cast<LPARAM>(command.size()));
}

void MainWindow::SubmitServerMapChange()
{
	FString map = ServerGuiUtf8FromWindow(ServerMap);
	map.StripLeftRight();
	if (!ServerGuiIsSafeCommandToken(map))
	{
		PrintStr("HCDE server console: enter a valid map lump name such as MAP01 or E1M1.\n");
		return;
	}

	FString command;
	command.Format("changemap %s", map.GetChars());
	SendServerConsoleCommand(command.GetChars());
}

void MainWindow::SubmitServerBroadcast()
{
	FString message = ServerGuiUtf8FromWindow(ServerBroadcast);
	message.StripLeftRight();
	if (message.IsEmpty())
	{
		PrintStr("HCDE server console: enter a message before broadcasting.\n");
		return;
	}

	FString command;
	command.Format("say %s", ServerGuiQuotedConsoleString(message.GetChars()).GetChars());
	SetWindowTextW(ServerBroadcast, L"");
	SendServerConsoleCommand(command.GetChars());
}

void MainWindow::RefreshServerKickList()
{
	if (ServerKick == 0)
	{
		return;
	}

	TArray<int> clients;
	TArray<FString> labels;
	Net_GetKickableClientList(clients, labels);

	bool listChanged = clients.Size() != ServerKickClients.Size() || labels.Size() != ServerKickClientLabels.Size();
	if (!listChanged)
	{
		for (unsigned int i = 0; i < clients.Size(); ++i)
		{
			if (clients[i] != ServerKickClients[i]
				|| strcmp(labels[i].GetChars(), ServerKickClientLabels[i].GetChars()) != 0)
			{
				listChanged = true;
				break;
			}
		}
	}

	if (!listChanged)
	{
		return;
	}

	int previousClient = -1;
	int previousIndex = ServerGuiSelectedComboIndex(ServerKick);
	if (previousIndex >= 0 && previousIndex < static_cast<int>(ServerKickClients.Size()))
	{
		previousClient = ServerKickClients[previousIndex];
	}

	ServerKickClients.Clear();
	ServerKickClientLabels.Clear();
	SendMessage(ServerKick, CB_RESETCONTENT, 0, 0);

	int selectedIndex = 0;
	for (unsigned int i = 0; i < clients.Size(); ++i)
	{
		ServerKickClients.Push(clients[i]);
		ServerKickClientLabels.Push(labels[i]);

		if (clients[i] == previousClient)
		{
			selectedIndex = static_cast<int>(i);
		}

		std::wstring wide = ServerGuiWideString(labels[i].GetChars());
		SendMessageW(ServerKick, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
	}

	if (ServerKickClients.Size() == 0)
	{
		SendMessageW(ServerKick, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"No players connected"));
	}

	SendMessage(ServerKick, CB_SETCURSEL, selectedIndex, 0);
	EnableWindow(GetDlgItem(Window, IDC_HCDE_SERVER_KICK_SEND), ServerKickClients.Size() > 0);
}

void MainWindow::SubmitServerKick()
{
	RefreshServerKickList();

	int index = ServerGuiSelectedComboIndex(ServerKick);
	if (index < 0 || index >= static_cast<int>(ServerKickClients.Size()))
	{
		PrintStr("HCDE server console: choose a connected player before kicking.\n");
		return;
	}

	FString command;
	command.Format("kick %d", ServerKickClients[index]);
	SendServerConsoleCommand(command.GetChars());
}

void MainWindow::ApplyServerConsolePreset()
{
	int index = ServerGuiSelectedComboIndex(ServerPresetCombo);
	if (index < 0 || index >= ServerGuiPresetCount)
	{
		PrintStr("HCDE server console: choose a preset before applying it.\n");
		return;
	}

	const ServerGuiPresetDefinition& preset = ServerGuiPresets[index];
	FString message;
	message.Format("HCDE server console: applying preset '%s'.\n", preset.LogName);
	PrintStr(message.GetChars());
	for (int i = 0; i < preset.CommandCount; ++i)
	{
		SendServerConsoleCommand(preset.Commands[i]);
	}
}

void MainWindow::ApplyServerConsoleFlag(bool enabled)
{
	int index = ServerGuiSelectedComboIndex(ServerFlagCombo);
	if (index < 0 || index >= ServerGuiFlagCount)
	{
		PrintStr("HCDE server console: choose a flag before toggling it.\n");
		return;
	}

	const ServerGuiFlagDefinition& flag = ServerGuiFlags[index];
	FString message;
	message.Format("HCDE server console: setting '%s' %s.\n", flag.LogName, enabled ? "on" : "off");
	PrintStr(message.GetChars());

	FString command;
	if (strcmp(flag.CVarName, "sv_usemasters") == 0)
	{
		command = enabled ? "advertise" : "unadvertise";
	}
	else
	{
		command.Format("%s %d", flag.CVarName, enabled ? 1 : 0);
	}
	SendServerConsoleCommand(command.GetChars());
}

void MainWindow::ClearServerConsoleLog()
{
	if (ServerLog == 0)
	{
		return;
	}

	SetWindowTextW(ServerLog, L"");
	PrintStr("HCDE server console: visible log cleared.\n");
}

void MainWindow::CopyServerConsoleLog()
{
	if (ServerLog == 0)
	{
		return;
	}

	const int length = GetWindowTextLengthW(ServerLog);
	if (length <= 0)
	{
		PrintStr("HCDE server console: visible log is empty.\n");
		return;
	}

	HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (static_cast<size_t>(length) + 1u) * sizeof(wchar_t));
	if (memory == nullptr)
	{
		PrintStr("HCDE server console: failed to copy log; clipboard allocation failed.\n");
		return;
	}

	wchar_t* text = static_cast<wchar_t*>(GlobalLock(memory));
	if (text == nullptr)
	{
		GlobalFree(memory);
		PrintStr("HCDE server console: failed to copy log; clipboard memory lock failed.\n");
		return;
	}
	GetWindowTextW(ServerLog, text, length + 1);
	GlobalUnlock(memory);

	if (!OpenClipboard(Window))
	{
		GlobalFree(memory);
		PrintStr("HCDE server console: failed to copy log; clipboard is busy.\n");
		return;
	}

	EmptyClipboard();
	if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr)
	{
		CloseClipboard();
		GlobalFree(memory);
		PrintStr("HCDE server console: failed to copy log; clipboard rejected the data.\n");
		return;
	}

	CloseClipboard();
	PrintStr("HCDE server console: copied visible log to clipboard.\n");
}

void MainWindow::RequestServerConsoleStart()
{
	PrintStr("HCDE server console: start requested.\n");
	SetServerConsoleStatus("start requested");
	I_DedicatedServerRequestStart();
}

void MainWindow::RequestServerConsoleShutdown()
{
	PrintStr("HCDE server console: stop requested.\n");
	SetServerConsoleStatus("stop requested");
	I_DedicatedServerRequestAbort();
	SendServerConsoleCommand("quit");
}

void MainWindow::ApplyServerConsoleSetting(int applyId)
{
	int index = applyId - IDC_HCDE_SERVER_SETTING_APPLY_BASE;
	if (index < 0 || index >= ServerSettingCount || index >= ServerGuiSettingCount || ServerSettingInputs[index] == 0)
	{
		PrintStr("HCDE server console: unknown server setting.\n");
		return;
	}

	const ServerGuiSettingDefinition& setting = ServerGuiSettings[index];
	FString value = ServerGuiValueFromControl(ServerSettingInputs[index], setting);
	FString command;
	FString warning;
	if (!ServerGuiBuildSettingCommand(setting, value, command, warning))
	{
		if (!warning.IsEmpty())
		{
			PrintStr(warning.GetChars());
		}
		return;
	}

	SendServerConsoleCommand(command.GetChars());
}

void MainWindow::ApplyServerConsoleVisibleSettings()
{
	TArray<FString> commands;
	for (int i = 0; i < ServerSettingCount && i < ServerGuiSettingCount; ++i)
	{
		if (ServerSettingInputs[i] == 0)
		{
			PrintStr("HCDE server console: cannot apply all settings because a visible setting control is missing.\n");
			return;
		}

		const ServerGuiSettingDefinition& setting = ServerGuiSettings[i];
		FString value = ServerGuiValueFromControl(ServerSettingInputs[i], setting);
		FString command;
		FString warning;
		if (!ServerGuiBuildSettingCommand(setting, value, command, warning))
		{
			if (!warning.IsEmpty())
			{
				PrintStr(warning.GetChars());
			}
			PrintStr("HCDE server console: apply-all cancelled; no settings were changed.\n");
			return;
		}
		commands.Push(command);
	}

	FString message;
	message.Format("HCDE server console: applying %u visible server settings.\n", commands.Size());
	PrintStr(message.GetChars());
	for (const FString& command : commands)
	{
		SendServerConsoleCommand(command.GetChars());
	}
}

void MainWindow::ApplyServerConsoleAdvancedSetting()
{
	int index = ServerGuiSelectedComboIndex(ServerAdvancedSettingCombo);
	if (index < 0 || index >= ServerGuiAdvancedSettingCount || ServerAdvancedSettingValue == 0 || ServerAdvancedSettingValueCombo == 0)
	{
		PrintStr("HCDE server console: choose an advanced CVar before applying it.\n");
		return;
	}

	const ServerGuiSettingDefinition& setting = ServerGuiAdvancedSettings[index];
	HWND valueControl = setting.Kind == ServerGuiSettingKind::Choice ? ServerAdvancedSettingValueCombo : ServerAdvancedSettingValue;
	FString value = ServerGuiValueFromControl(valueControl, setting);
	FString command;
	FString warning;
	if (!ServerGuiBuildSettingCommand(setting, value, command, warning))
	{
		if (!warning.IsEmpty())
		{
			PrintStr(warning.GetChars());
		}
		return;
	}

	SendServerConsoleCommand(command.GetChars());
}

void MainWindow::SyncServerConsoleSettingsFromCVars(bool logResult)
{
	for (int i = 0; i < ServerSettingCount && i < ServerGuiSettingCount; ++i)
	{
		ServerGuiSetControlFromCVar(ServerSettingInputs[i], ServerGuiSettings[i]);
	}

	UpdateServerConsoleAdvancedSettingDefault();

	if (logResult)
	{
		PrintStr("HCDE server console: refreshed visible controls from current CVars.\n");
	}
}

void MainWindow::UpdateServerConsoleAdvancedSettingDefault()
{
	int index = ServerGuiSelectedComboIndex(ServerAdvancedSettingCombo);
	if (index < 0 || index >= ServerGuiAdvancedSettingCount || ServerAdvancedSettingValue == 0 || ServerAdvancedSettingValueCombo == 0)
	{
		return;
	}

	const ServerGuiSettingDefinition& setting = ServerGuiAdvancedSettings[index];
	if (setting.Kind == ServerGuiSettingKind::Choice)
	{
		ShowWindow(ServerAdvancedSettingValue, SW_HIDE);
		ShowWindow(ServerAdvancedSettingValueCombo, SW_SHOW);
		ServerGuiPopulateChoiceControl(ServerAdvancedSettingValueCombo, setting);
		ServerGuiSetControlFromCVar(ServerAdvancedSettingValueCombo, setting);
	}
	else
	{
		ShowWindow(ServerAdvancedSettingValueCombo, SW_HIDE);
		ShowWindow(ServerAdvancedSettingValue, SW_SHOW);
		ServerGuiSetControlFromCVar(ServerAdvancedSettingValue, setting);
	}
}

void MainWindow::UpdateServerConsoleStatusFromLogLine(const char* text)
{
	if (text == nullptr || *text == 0)
	{
		return;
	}

	if (strstr(text, "Execution could not continue") != nullptr ||
		strstr(text, "Fatal Error") != nullptr)
	{
		SetServerConsoleStatus("error - see log");
	}
	else if (strstr(text, "D_CheckNetGame") != nullptr)
	{
		SetServerConsoleStatus("preparing network");
	}
	else if (strstr(text, "Starting dedicated server") != nullptr)
	{
		SetServerConsoleStatus("starting dedicated server");
	}
	else if (strstr(text, "Dedicated server accepting clients") != nullptr)
	{
		SetServerConsoleStatus("accepting clients");
	}
	else if (strstr(text, "Starting dedicated game") != nullptr)
	{
		SetServerConsoleStatus("starting game");
	}
	else if (strstr(text, "Client ") != nullptr && strstr(text, " connected") != nullptr)
	{
		SetServerConsoleStatus("client connected");
	}
	else if (strstr(text, "Client ") != nullptr && strstr(text, " disconnected") != nullptr)
	{
		SetServerConsoleStatus("client disconnected");
	}
}

void I_SetDedicatedServerConsoleStatus(const char* status)
{
	mainwindow.SetServerConsoleStatus(status);
}
#endif
