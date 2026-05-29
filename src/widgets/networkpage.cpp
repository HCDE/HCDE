/*
** networkpage.cpp
**
**
**
**---------------------------------------------------------------------------
**
** Copyright 2024 Magnus Norddahl
** Copyright 2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

#include <zwidget/core/resourcedata.h>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include <zwidget/widgets/dropdown/dropdown.h>
#include <zwidget/widgets/lineedit/lineedit.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/tabwidget/tabwidget.h>
#include <zwidget/widgets/textlabel/textlabel.h>

#include "gstrings.h"
#include "i_interface.h"
#include "i_net.h"
#include "launcherwindow.h"
#include "networkpage.h"
#include "widgets/themedata.h"

constexpr double EditHeight = 24.0;

namespace
{
FString BuildServerInfoText(const FServerQuerySnapshot& snapshot)
{
	FString info;
	const char* modeName = snapshot.GameModeName.IsNotEmpty()
		? snapshot.GameModeName.GetChars()
		: (snapshot.Deathmatch ? (snapshot.Teamplay ? "Deathmatch + Teamplay" : "Deathmatch") : (snapshot.Teamplay ? "Co-op + Teamplay" : "Co-op"));
	info.Format("Host: %s\nSession: %s\nMap: %s\nPlayers: %u/%u\nMode: %s\nSkill: %u\nTime left: %u\nFrag limit: %u\nVersion: %s (%s)",
		snapshot.HostName.GetChars(),
		snapshot.SessionState.GetChars(),
		snapshot.MapName.GetChars(),
		snapshot.PlayerCount,
		snapshot.MaxPlayers,
		modeName,
		snapshot.Skill,
		snapshot.TimeLeft,
		snapshot.FragLimit,
		snapshot.Version.GetChars(),
		snapshot.GitHash.GetChars());

	if (snapshot.Players.Size() > 0)
	{
		info.AppendFormat("\nPlayers:");
		for (size_t i = 0; i < snapshot.Players.Size(); ++i)
		{
			const auto& player = snapshot.Players[i];
			info.AppendFormat("\n%u. %s | %ums | %d/%d/%d",
				static_cast<unsigned>(i + 1),
				player.Name.GetChars(),
				player.Ping,
				player.Frags,
				player.Kills,
				player.Deaths);
		}
	}
	else
	{
		info.AppendFormat("\nPlayers: none");
	}

	if (snapshot.GameMode == 4 && snapshot.InvasionStateName.IsNotEmpty())
	{
		constexpr unsigned QueryTicRate = 35u;
		const unsigned seconds = static_cast<unsigned>((snapshot.InvasionStateTics + QueryTicRate - 1) / QueryTicRate);
		info.AppendFormat("\nInvasion: %s (%us)", snapshot.InvasionStateName.GetChars(), seconds);
		if (snapshot.InvasionMaxWaves > 0)
		{
			const bool bossWave = (snapshot.InvasionWaveFlags & 1u) != 0u;
			info.AppendFormat("\nWave: %u/%u | Budget: %u | Spawned: %u | Cleared: %u | Active: %u%s",
				snapshot.InvasionWave,
				snapshot.InvasionMaxWaves,
				snapshot.InvasionWaveBudget,
				snapshot.InvasionWaveSpawned,
				snapshot.InvasionWaveCleared,
				snapshot.InvasionActiveMonsters,
				bossWave ? " | Boss wave" : "");
		}
		if (snapshot.InvasionSpawnSpotCount > 0)
		{
			const bool fallback = (snapshot.InvasionSpawnFlags & 1u) != 0u;
			const uint8_t source = uint8_t((snapshot.InvasionSpawnFlags >> 1) & 0x07u);
			const char* sourceName = "none";
			switch (source)
			{
			case 1: sourceName = "classic"; break;
			case 2: sourceName = "mapspot"; break;
			case 3: sourceName = "deathmatch"; break;
			case 4: sourceName = "playerstart"; break;
			default: break;
			}

			info.AppendFormat("\nSpots: %u total | %u active | Plan: %u",
				snapshot.InvasionSpawnSpotCount,
				snapshot.InvasionSpawnActiveSpotCount,
				snapshot.InvasionSpawnPlanBudget);
			if (snapshot.InvasionSpawnActiveTag > 0)
				info.AppendFormat(" | Tag: %u", snapshot.InvasionSpawnActiveTag);
			if (fallback)
				info.AppendFormat(" | Fallback: %s", sourceName);
		}
	}

	return info;
}
}

NetworkPage::NetworkPage(LauncherWindow* launcher, const FStartupSelectionInfo& info) : Widget(nullptr), Launcher(launcher)
{
	ParametersEdit = new LineEdit(this);
	ParametersLabel = new TextLabel(this);
	SaveFileEdit = new LineEdit(this);
	SaveFileLabel = new TextLabel(this);
	SaveFileCheckbox = new CheckboxLabel(this);
	SaveParametersCheckbox = new CheckboxLabel(this);
	IWADsDropdown = new Dropdown(this);
	PlayerClassLabel = new TextLabel(this);
	PlayerClassEdit = new LineEdit(this);

	SaveFileCheckbox->SetChecked(info.bSaveNetFile);
	if (!info.DefaultNetSaveFile.IsEmpty())
		SaveFileEdit->SetText(info.DefaultNetSaveFile.GetChars());

	SaveParametersCheckbox->SetChecked(info.bSaveNetArgs);
	if (!info.DefaultNetArgs.IsEmpty())
		ParametersEdit->SetText(info.DefaultNetArgs.GetChars());

	StartPages = new TabWidget(this);
	HostPage = new HostSubPage(this, info);
	JoinPage = new JoinSubPage(this, info);
	StartPages->OnCurrentChanged = [this]() { OnStartPageChanged(); };
	IWADsDropdown->OnChanged = [this](int) { UpdatePlayButton(); };
	ParametersEdit->FuncAfterEditChanged = [this]() { UpdatePlayButton(); };
	SaveFileEdit->FuncAfterEditChanged = [this]() { UpdatePlayButton(); };
	SaveFileCheckbox->FuncChanged = [this](bool) { UpdatePlayButton(); };
	SaveParametersCheckbox->FuncChanged = [this](bool) { UpdatePlayButton(); };
	PlayerClassEdit->FuncAfterEditChanged = [this]() { UpdatePlayButton(); };

	for (const auto& wad : *info.Wads)
	{
		const char* filepart = strrchr(wad.Path.GetChars(), '/');
		if (filepart == nullptr)
			filepart = wad.Path.GetChars();
		else
			++filepart;

		FString work;
		if (*filepart)
			work.Format("%s (%s)", wad.Name.GetChars(), filepart);
		else
			work = wad.Name.GetChars();

		IWADsDropdown->AddItem(work.GetChars());
	}

	if (info.DefaultNetIWAD >= 0 && info.DefaultNetIWAD < info.Wads->SSize())
		IWADsDropdown->SetSelectedItem(info.DefaultNetIWAD);
}

// This has to be done after the main page is parented, otherwise it won't have the correct
// info to pull from.
void NetworkPage::InitializeTabs(const FStartupSelectionInfo& info)
{
	StartPages->AddTab(HostPage, "Host");
	StartPages->AddTab(JoinPage, "Join");

	switch (info.DefaultNetPage)
	{
	case 1:
		StartPages->SetCurrentWidget(JoinPage);
		break;
	default:
		StartPages->SetCurrentWidget(HostPage);
		break;
	}
}

void NetworkPage::OnIWADsListActivated()
{
	Launcher->Start();
}

void NetworkPage::OnStartPageChanged()
{
	if (StartPages != nullptr && StartPages->GetCurrentWidget() == JoinPage)
		JoinPage->QueryServerInfo();
}

void NetworkPage::OnSetFocus()
{
	IWADsDropdown->SetFocus();
}

void NetworkPage::SetValues(FStartupSelectionInfo& info) const
{
	info.DefaultNetIWAD = IWADsDropdown->GetSelectedItem();
	info.DefaultNetArgs = ParametersEdit->GetText();

	info.bHosting = IsInHost();
	if (info.bHosting)
	{
		info.DefaultNetPage = 0;
		HostPage->SetValues(info);
	}
	else
	{
		info.DefaultNetPage = 1;
		JoinPage->SetValues(info);
	}

	info.bSaveNetFile = SaveFileCheckbox->GetChecked();
	info.bSaveNetArgs = SaveParametersCheckbox->GetChecked();
	const auto save = SaveFileEdit->GetText();
	if (!save.empty())
		info.AdditionalNetArgs.AppendFormat(" -loadgame \"%s\"", save.c_str());
	const auto pClass = PlayerClassEdit->GetText();
	if (!pClass.empty())
		info.AdditionalNetArgs.AppendFormat(" +playerclass \"%s\"", pClass.c_str());
	info.DefaultNetSaveFile = save;
}

void NetworkPage::UpdatePlayButton()
{
	Launcher->UpdatePlayButton();
}

bool NetworkPage::IsInHost() const
{
	return StartPages->GetCurrentIndex() >= 0 ? StartPages->GetCurrentWidget() == HostPage : false;
}

void NetworkPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	double y = 0.0;
	IWADsDropdown->SetFrameGeometry(0.0, y, w, IWADsDropdown->GetPreferredHeight());
	y += IWADsDropdown->GetPreferredHeight() + 7.0;
	const double pageTop = y;

	y = h - SaveFileCheckbox->GetPreferredHeight();
	SaveFileCheckbox->SetFrameGeometry(0.0, y, w, SaveFileCheckbox->GetPreferredHeight());

	y -= SaveParametersCheckbox->GetPreferredHeight();
	SaveParametersCheckbox->SetFrameGeometry(0.0, y, w, SaveParametersCheckbox->GetPreferredHeight());

	y -= EditHeight + 2.0;
	ParametersEdit->SetFrameGeometry(0.0, y, w, EditHeight);
	y -= ParametersLabel->GetPreferredHeight();
	ParametersLabel->SetFrameGeometry(0.0, y, w, ParametersLabel->GetPreferredHeight());

	const double wSize = w * 0.5;

	y -= EditHeight + 2.0;
	const double top = y;
	SaveFileEdit->SetFrameGeometry(0.0, y, wSize - 2.5, EditHeight);
	y -= SaveFileLabel->GetPreferredHeight();
	SaveFileLabel->SetFrameGeometry(0.0, y, wSize - 2.5, SaveFileLabel->GetPreferredHeight());

	y = top;
	PlayerClassEdit->SetFrameGeometry(wSize + 2.5, y, wSize - 2.5, EditHeight);
	y -= PlayerClassLabel->GetPreferredHeight();
	PlayerClassLabel->SetFrameGeometry(wSize + 2.5, y, wSize - 2.5, PlayerClassLabel->GetPreferredHeight());

	StartPages->SetFrameGeometry(0.0, pageTop, w, y - pageTop);
}

void NetworkPage::UpdateLanguage()
{
	ParametersLabel->SetText(GStrings.GetString("PICKER_ADDPARM"));
	SaveFileLabel->SetText(GStrings.GetString("PICKER_LOADSAVE"));
	SaveFileCheckbox->SetText(GStrings.GetString("PICKER_REMSAVE"));
	SaveParametersCheckbox->SetText(GStrings.GetString("PICKER_REMPARM"));
	PlayerClassLabel->SetText(GStrings.GetString("PICKER_PLAYERCLASS"));

	StartPages->SetTabText(HostPage, GStrings.GetString("PICKER_HOST"));
	StartPages->SetTabText(JoinPage, GStrings.GetString("PICKER_JOIN"));
	HostPage->UpdateLanguage();
	JoinPage->UpdateLanguage();
}

HostSubPage::HostSubPage(NetworkPage* main, const FStartupSelectionInfo& info) : Widget(nullptr), MainTab(main)
{
	TicDupLabel = new TextLabel(this);
	TicDupDropdown = new Dropdown(this);
	ExtraTicCheckbox = new CheckboxLabel(this);

	TicDupDropdown->AddItem("35 Hz");
	TicDupDropdown->AddItem("17.5 Hz");
	TicDupDropdown->AddItem("11.6 Hz");
	TicDupDropdown->SetSelectedItem(max<int>(info.DefaultNetTicDup, 0));

	ExtraTicCheckbox->SetChecked(info.DefaultNetExtraTic);

	GameModesLabel = new TextLabel(this);
	GameModesDropdown = new Dropdown(this);
	AltDeathmatchCheckbox = new CheckboxLabel(this);

	GameModesDropdown->AddItem("None");
	GameModesDropdown->AddItem("Co-op");
	GameModesDropdown->AddItem("Deathmatch");
	GameModesDropdown->AddItem("Team Deathmatch");
	GameModesDropdown->AddItem("Invasion");
	GameModesDropdown->SetSelectedItem(clamp<int>(info.DefaultNetGameMode, 0, 4));

	TeamLabel = new TextLabel(this);
	TeamEdit = new LineEdit(this);

	AltDeathmatchCheckbox->SetChecked(info.DefaultNetAltDM);

	TeamEdit->SetMaxLength(3);
	TeamEdit->SetNumericMode(true);
	TeamEdit->SetTextInt(info.DefaultNetHostTeam);

	MaxPlayersEdit = new LineEdit(this);
	PortEdit = new LineEdit(this);
	MaxPlayersLabel = new TextLabel(this);
	PortLabel = new TextLabel(this);

	MaxPlayersEdit->SetMaxLength(2);
	MaxPlayersEdit->SetNumericMode(true);
	MaxPlayersEdit->SetTextInt(info.DefaultNetPlayers);
	PortEdit->SetMaxLength(5);
	PortEdit->SetNumericMode(true);
	if (info.DefaultNetHostPort > 0)
		PortEdit->SetTextInt(info.DefaultNetHostPort);

	MaxPlayerHintLabel = new TextLabel(this);
	PortHintLabel = new TextLabel(this);
	TeamHintLabel = new TextLabel(this);

	MaxPlayerHintLabel->SetStyleColor("color", Theme::getMain(COLOR_MIX));
	PortHintLabel->SetStyleColor("color", Theme::getMain(COLOR_MIX));
	TeamHintLabel->SetStyleColor("color", Theme::getMain(COLOR_MIX));

	TicDupDropdown->OnChanged = [this](int) { MainTab->UpdatePlayButton(); };
	ExtraTicCheckbox->FuncChanged = [this](bool) { MainTab->UpdatePlayButton(); };
	GameModesDropdown->OnChanged = [this](int) { MainTab->UpdatePlayButton(); };
	AltDeathmatchCheckbox->FuncChanged = [this](bool) { MainTab->UpdatePlayButton(); };
	TeamEdit->FuncAfterEditChanged = [this]() { MainTab->UpdatePlayButton(); };
	MaxPlayersEdit->FuncAfterEditChanged = [this]() { MainTab->UpdatePlayButton(); };
	PortEdit->FuncAfterEditChanged = [this]() { MainTab->UpdatePlayButton(); };
}

void HostSubPage::SetValues(FStartupSelectionInfo& info) const
{
	info.AdditionalNetArgs = "";

	info.DefaultNetExtraTic = ExtraTicCheckbox->GetChecked();
	if (info.DefaultNetExtraTic)
		info.AdditionalNetArgs.AppendFormat(" -extratic");

	const int dup = TicDupDropdown->GetSelectedItem();
	if (dup > 0)
		info.AdditionalNetArgs.AppendFormat(" -dup %d", dup + 1);
	info.DefaultNetTicDup = dup;

	info.DefaultNetPlayers = clamp<int>(MaxPlayersEdit->GetTextInt(), 1, MAXPLAYERS);
	info.AdditionalNetArgs.AppendFormat(" -host %d", info.DefaultNetPlayers);
	const int port = clamp<int>(PortEdit->GetTextInt(), 0, UINT16_MAX);
	if (port > 0)
	{
		info.AdditionalNetArgs.AppendFormat(" -port %d", port);
		info.DefaultNetHostPort = port;
	}
	else
	{
		info.DefaultNetHostPort = 0;
	}

	info.DefaultNetAltDM = AltDeathmatchCheckbox->GetChecked();
	info.DefaultNetGameMode = GameModesDropdown->GetSelectedItem();
	switch (info.DefaultNetGameMode)
	{
	case 1:
		info.AdditionalNetArgs.AppendFormat(" -coop +sv_gametype 0");
		break;
	case 3:
		{
			info.AdditionalNetArgs.AppendFormat(" +teamplay 1 +sv_gametype 2");
			int team = 255;
			if (!TeamEdit->GetText().empty())
			{
				team = TeamEdit->GetTextInt();
				if (team < 0 || team > 255)
					team = 255;
			}
			info.AdditionalNetArgs.AppendFormat(" +team %d", team);
			info.DefaultNetHostTeam = team;
		}
		break;
	case 2:
		if (AltDeathmatchCheckbox->GetChecked())
			info.AdditionalNetArgs.AppendFormat(" -altdeath +sv_gametype 1");
		else
			info.AdditionalNetArgs.AppendFormat(" -deathmatch +sv_gametype 1");
		break;
	case 4:
		info.AdditionalNetArgs.AppendFormat(" +sv_gametype 4");
		break;
	case 0:
	default:
		info.AdditionalNetArgs.AppendFormat(" -coop +sv_gametype 0");
		break;
	}
}

void HostSubPage::UpdateLanguage()
{
	TicDupLabel->SetText(GStrings.GetString("PICKER_NETRATE"));
	ExtraTicCheckbox->SetText(GStrings.GetString("PICKER_NETBACKUP"));

	GameModesLabel->SetText(GStrings.GetString("PICKER_GAMEMODE"));
	GameModesDropdown->UpdateItem(GStrings.GetString("OPTVAL_NONE"), 0);
	GameModesDropdown->UpdateItem(GStrings.GetString("PICKER_COOP"), 1);
	GameModesDropdown->UpdateItem(GStrings.GetString("PICKER_DM"), 2);
	GameModesDropdown->UpdateItem(GStrings.GetString("PICKER_TDM"), 3);
	GameModesDropdown->UpdateItem("Invasion", 4);
	AltDeathmatchCheckbox->SetText(GStrings.GetString("PICKER_ALTDM"));
	TeamLabel->SetText(GStrings.GetString("PICKER_TEAM"));

	MaxPlayersLabel->SetText(GStrings.GetString("PICKER_PLAYERS"));
	PortLabel->SetText(GStrings.GetString("PICKER_PORT"));

	MaxPlayerHintLabel->SetText(GStrings.GetString("PICKER_PLAYERHINT"));
	PortHintLabel->SetText(GStrings.GetString("PICKER_PORTHINT"));
	TeamHintLabel->SetText(GStrings.GetString("PICKER_TEAMHINT"));
}

void HostSubPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	constexpr double LabelOfsSize = 100.0;
	constexpr double hintOfs = 160.0;
	constexpr double DropdownSize = 220.0;
	constexpr double EditWidth = 55.0;

	double y = 0.0;
	const double wSize = w * 0.5;

	MaxPlayersLabel->SetFrameGeometry(0.0, y, LabelOfsSize, MaxPlayersLabel->GetPreferredHeight());
	MaxPlayersEdit->SetFrameGeometry(LabelOfsSize, y, EditWidth, EditHeight);
	y += EditHeight + 2.0;

	PortLabel->SetFrameGeometry(0.0, y, LabelOfsSize, PortLabel->GetPreferredHeight());
	PortEdit->SetFrameGeometry(LabelOfsSize, y, EditWidth, EditHeight);

	MaxPlayerHintLabel->SetFrameGeometry(hintOfs, 0.0, wSize - hintOfs, MaxPlayerHintLabel->GetPreferredHeight());
	PortHintLabel->SetFrameGeometry(hintOfs, y, wSize - hintOfs, PortHintLabel->GetPreferredHeight());

	y += EditHeight + 7.0;

	GameModesLabel->SetFrameGeometry(0.0, y, wSize, GameModesLabel->GetPreferredHeight());
	y += GameModesLabel->GetPreferredHeight();
	GameModesDropdown->SetFrameGeometry(0.0, y, DropdownSize, GameModesDropdown->GetPreferredHeight());
	y += GameModesDropdown->GetPreferredHeight() + 2.0;

	TeamLabel->SetFrameGeometry(0.0, y, LabelOfsSize, TeamLabel->GetPreferredHeight());
	TeamEdit->SetFrameGeometry(LabelOfsSize, y, EditWidth, EditHeight);
	TeamHintLabel->SetFrameGeometry(hintOfs, y, wSize - hintOfs, TeamHintLabel->GetPreferredHeight());
	y += EditHeight + 2.0;

	AltDeathmatchCheckbox->SetFrameGeometry(0.0, y, wSize, AltDeathmatchCheckbox->GetPreferredHeight());

	y = 0.0;

	TicDupLabel->SetFrameGeometry(w - DropdownSize, y, DropdownSize, TicDupLabel->GetPreferredHeight());
	y += TicDupLabel->GetPreferredHeight();
	TicDupDropdown->SetFrameGeometry(w - DropdownSize, y, DropdownSize, TicDupDropdown->GetPreferredHeight());
	y += TicDupDropdown->GetPreferredHeight() + 2.0;

	ExtraTicCheckbox->SetFrameGeometry(w - DropdownSize, y, DropdownSize, ExtraTicCheckbox->GetPreferredHeight());

	MainTab->UpdatePlayButton();
}

JoinSubPage::JoinSubPage(NetworkPage* main, const FStartupSelectionInfo& info) : Widget(nullptr), MainTab(main)
{
	AddressEdit = new LineEdit(this);
	AddressPortEdit = new LineEdit(this);
	AddressLabel = new TextLabel(this);
	AddressPortLabel = new TextLabel(this);
	QueryButton = new PushButton(this);
	ServerInfoLabel = new TextLabel(this);

	AddressEdit->SetText(info.DefaultNetAddress.GetChars());
	AddressPortEdit->SetMaxLength(5);
	AddressPortEdit->SetNumericMode(true);
	if (info.DefaultNetJoinPort > 0)
		AddressPortEdit->SetTextInt(info.DefaultNetJoinPort);

	TeamDeathmatchLabel = new TextLabel(this);
	TeamLabel = new TextLabel(this);
	TeamEdit = new LineEdit(this);

	TeamEdit->SetMaxLength(3);
	TeamEdit->SetNumericMode(true);
	TeamEdit->SetTextInt(info.DefaultNetJoinTeam);

	AddressPortHintLabel = new TextLabel(this);
	TeamHintLabel = new TextLabel(this);

	AddressPortHintLabel->SetStyleColor("color", Theme::getMain(COLOR_MIX));
	TeamHintLabel->SetStyleColor("color", Theme::getMain(COLOR_MIX));
	ServerInfoLabel->SetStyleColor("color", Theme::getMain(COLOR_MIX));

	QueryButton->OnClick = [this]() { QueryServerInfo(); };
	AddressEdit->FuncEnterPressed = [this]() { QueryServerInfo(); };
	AddressPortEdit->FuncEnterPressed = [this]() { QueryServerInfo(); };
	AddressEdit->FuncAfterEditChanged = [this]() { MainTab->UpdatePlayButton(); };
	AddressPortEdit->FuncAfterEditChanged = [this]() { MainTab->UpdatePlayButton(); };
	TeamEdit->FuncAfterEditChanged = [this]() { MainTab->UpdatePlayButton(); };
	QueryButton->SetText("Refresh");
	ServerInfoLabel->SetText("Press Refresh to query the server.");
}

void JoinSubPage::SetValues(FStartupSelectionInfo& info) const
{
	FString addr = AddressEdit->GetText();
	info.DefaultNetAddress = addr;
	const int port = clamp<int>(AddressPortEdit->GetTextInt(), 0, UINT16_MAX);
	if (port > 0)
	{
		addr.AppendFormat(":%d", port);
		info.DefaultNetJoinPort = port;
	}
	else
	{
		info.DefaultNetJoinPort = 0;
	}

	info.AdditionalNetArgs = "";
	info.AdditionalNetArgs.AppendFormat(" -join %s", addr.GetChars());

	int team = 255;
	if (!TeamEdit->GetText().empty())
	{
		team = TeamEdit->GetTextInt();
		if (team < 0 || team > 255)
			team = 255;
	}
	info.AdditionalNetArgs.AppendFormat(" +team %d", team);
	info.DefaultNetJoinTeam = team;
}

void JoinSubPage::QueryServerInfo()
{
	FString addr = AddressEdit->GetText();
	const int port = clamp<int>(AddressPortEdit->GetTextInt(), 0, UINT16_MAX);
	if (port > 0)
		addr.AppendFormat(":%d", port);

	FString error = {};
	FServerQuerySnapshot snapshot = {};
	if (I_QueryServerInfo(addr.GetChars(), snapshot, &error))
		ServerInfoLabel->SetText(BuildServerInfoText(snapshot).GetChars());
	else
		ServerInfoLabel->SetText(FStringf("Query failed: %s", error.GetChars()).GetChars());
}

void JoinSubPage::UpdateLanguage()
{
	AddressLabel->SetText(GStrings.GetString("PICKER_IP"));
	AddressPortLabel->SetText(GStrings.GetString("PICKER_PORT"));
	QueryButton->SetText("Refresh");

	TeamDeathmatchLabel->SetText(GStrings.GetString("PICKER_TDMLABEL"));
	TeamLabel->SetText(GStrings.GetString("PICKER_TEAM"));

	AddressPortHintLabel->SetText(GStrings.GetString("PICKER_PORTHINT"));
	TeamHintLabel->SetText(GStrings.GetString("PICKER_TEAMHINT"));
}

void JoinSubPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	constexpr double LabelOfsSize = 100.0;
	constexpr double hintOfs = 160.0;

	double y = 0.0;

	AddressLabel->SetFrameGeometry(0.0, y, LabelOfsSize, AddressLabel->GetPreferredHeight());
	AddressEdit->SetFrameGeometry(LabelOfsSize, y, std::max(w * 0.5 - LabelOfsSize - 2.5, 50.0), EditHeight);
	QueryButton->SetFrameGeometry(w * 0.5 + 2.5, y, std::max(w * 0.5 - 2.5, 90.0), EditHeight);
	y += EditHeight + 2.0;

	AddressPortLabel->SetFrameGeometry(0.0, y, LabelOfsSize, AddressPortLabel->GetPreferredHeight());
	AddressPortEdit->SetFrameGeometry(LabelOfsSize, y, 55.0, EditHeight);

	AddressPortHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, AddressPortHintLabel->GetPreferredHeight());
	y += EditHeight + 7.0;

	TeamDeathmatchLabel->SetFrameGeometry(0.0, y, w, TeamDeathmatchLabel->GetPreferredHeight());
	y += TeamDeathmatchLabel->GetPreferredHeight();

	TeamLabel->SetFrameGeometry(0.0, y, LabelOfsSize, TeamLabel->GetPreferredHeight());
	TeamEdit->SetFrameGeometry(LabelOfsSize, y, 55.0, EditHeight);
	TeamHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, TeamHintLabel->GetPreferredHeight());
	y += EditHeight + 2.0;

	const double infoTop = y + TeamHintLabel->GetPreferredHeight() + 6.0;
	ServerInfoLabel->SetFrameGeometry(0.0, infoTop, w, std::max(h - infoTop, 0.0));

	MainTab->UpdatePlayButton();
}
