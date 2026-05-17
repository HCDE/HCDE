/*
** playgamepage.cpp
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

#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include <zwidget/widgets/lineedit/lineedit.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/systemdialogs/open_file_dialog.h>

#include "gstrings.h"
#include "i_interface.h"
#include "launcherwindow.h"
#include "playgamepage.h"
#include "version.h"

PlayGamePage::PlayGamePage(LauncherWindow* launcher, const FStartupSelectionInfo& info) : Widget(nullptr), Launcher(launcher)
{
	WelcomeLabel = new TextLabel(this);
	VersionLabel = new TextLabel(this);
	VersionLabel->SetTextAlignment(TextLabelAlignment::Right);
	SelectLabel = new TextLabel(this);
	AddonsLabel = new TextLabel(this);
	ParametersLabel = new TextLabel(this);
	GamesList = new ListView(this);
	AddonsEdit = new LineEdit(this);
	AddonsBrowseButton = new PushButton(this);
	AddonsClearButton = new PushButton(this);
	ParametersEdit = new LineEdit(this);
	SaveArgsCheckbox = new CheckboxLabel(this);

	AddonsEdit->SetReadOnly();
	SaveArgsCheckbox->SetChecked(info.bSaveArgs);
	SetAddonFiles(info.DefaultAddonFiles);
	if (!info.DefaultArgs.IsEmpty())
		ParametersEdit->SetText(info.DefaultArgs.GetChars());

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

		GamesList->AddItem(work.GetChars());
	}

	if (info.DefaultIWAD >= 0 && info.DefaultIWAD < info.Wads->SSize())
	{
		GamesList->SetSelectedItem(info.DefaultIWAD);
	}

	GamesList->OnActivated = [this]() { OnGamesListActivated(); };
	AddonsBrowseButton->OnClick = [this]() { OnBrowseAddonsClicked(); };
	AddonsClearButton->OnClick = [this]() { OnClearAddonsClicked(); };
}

void PlayGamePage::SetValues(FStartupSelectionInfo& info) const
{
	info.DefaultIWAD = GamesList->GetSelectedItem();
	info.DefaultAddonFiles = GetAddonFiles();
	info.DefaultArgs = ParametersEdit->GetText();
	info.bSaveArgs = SaveArgsCheckbox->GetChecked();
}

void PlayGamePage::UpdateLanguage()
{
	SelectLabel->SetText(GStrings.GetString("PICKER_SELECT"));
	AddonsLabel->SetText(GStrings.GetString("PICKER_ADDONS"));
	AddonsBrowseButton->SetText(GStrings.GetString("PICKER_ADDONS_BROWSE"));
	AddonsClearButton->SetText(GStrings.GetString("PICKER_ADDONS_CLEAR"));
	ParametersLabel->SetText(GStrings.GetString("PICKER_ADDPARM"));
	FString welcomeText = GStrings.GetString("PICKER_WELCOME");
	welcomeText.Substitute("%s", GAMENAME);
	WelcomeLabel->SetText(welcomeText.GetChars());
	FString versionText = GStrings.GetString("PICKER_VERSION");
	versionText.Substitute("%s", GetVersionString());
	VersionLabel->SetText(versionText.GetChars());
	SaveArgsCheckbox->SetText(GStrings.GetString("PICKER_REMPARM"));
}

void PlayGamePage::OnGamesListActivated()
{
	Launcher->Start();
}

void PlayGamePage::OnBrowseAddonsClicked()
{
	auto dialog = OpenFileDialog::Create(this);
	dialog->SetTitle("Select PWAD / PK3 add-on files");
	dialog->SetMultiSelect(true);
	dialog->AddFilter("Doom add-ons (*.wad;*.pk3;*.pk7;*.pke;*.zip)", "*.wad;*.pk3;*.pk7;*.pke;*.zip");
	dialog->AddFilter("All files (*.*)", "*.*");

	if (!dialog->Show())
		return;

	for (const auto& filename : dialog->Filenames())
	{
		AddAddonFile(filename.c_str());
	}
	UpdateAddonFilesEdit();
}

void PlayGamePage::OnClearAddonsClicked()
{
	AddonFiles.Clear();
	UpdateAddonFilesEdit();
}

void PlayGamePage::OnSetFocus()
{
	GamesList->SetFocus();
}

bool PlayGamePage::OnFileDrop(std::string path)
{
	AddAddonFile(path.c_str());
	UpdateAddonFilesEdit();
	return true;
}

void PlayGamePage::AddAddonFile(FString path)
{
	path.StripLeftRight();
	if (path.IsEmpty())
		return;

	for (const auto& existing : AddonFiles)
	{
		if (!existing.CompareNoCase(path))
			return;
	}

	AddonFiles.Push(path);
}

void PlayGamePage::SetAddonFiles(FString files)
{
	AddonFiles.Clear();
	// Accept newline-delimited values from any temporary/test builds that saved
	// this setting before the launcher switched to a config-safe separator.
	files.Substitute("\r", StartupAddonFileSeparator);
	files.Substitute("\n", StartupAddonFileSeparator);
	TArray<FString> paths = files.Split(StartupAddonFileSeparator, FString::TOK_SKIPEMPTY);
	for (auto& path : paths)
	{
		AddAddonFile(path);
	}
	UpdateAddonFilesEdit();
}

FString PlayGamePage::GetAddonFiles() const
{
	FString files;
	for (const auto& file : AddonFiles)
	{
		if (files.IsNotEmpty())
			files += StartupAddonFileSeparator;
		files += file;
	}
	return files;
}

void PlayGamePage::UpdateAddonFilesEdit()
{
	FString text;
	for (const auto& file : AddonFiles)
	{
		if (text.IsNotEmpty())
			text += "; ";
		text += file;
	}
	AddonsEdit->SetText(text.GetChars());
}

void PlayGamePage::OnGeometryChanged()
{
	double y = 10.0;

	const double halfW = GetWidth() * 0.5;
	WelcomeLabel->SetFrameGeometry(0.0, y, halfW, WelcomeLabel->GetPreferredHeight());
	VersionLabel->SetFrameGeometry(halfW, y, halfW, VersionLabel->GetPreferredHeight());

	y += VersionLabel->GetPreferredHeight() + 10.0;

	SelectLabel->SetFrameGeometry(0.0, y, GetWidth(), SelectLabel->GetPreferredHeight());
	y += SelectLabel->GetPreferredHeight();

	const double listViewTop = y;

	y = GetHeight() - SaveArgsCheckbox->GetPreferredHeight();
	SaveArgsCheckbox->SetFrameGeometry(0.0, y, GetWidth(), SaveArgsCheckbox->GetPreferredHeight());

	const double editHeight = 24.0;
	y -= editHeight + 2.0;
	ParametersEdit->SetFrameGeometry(0.0, y, GetWidth(), editHeight);

	const double labelHeight = ParametersLabel->GetPreferredHeight();
	y -= labelHeight;
	ParametersLabel->SetFrameGeometry(0.0, y, GetWidth(), labelHeight);

	y -= 8.0;

	const double clearWidth = std::max(AddonsClearButton->GetPreferredWidth() + 10.0, 70.0);
	const double browseWidth = std::max(AddonsBrowseButton->GetPreferredWidth() + 10.0, 105.0);
	y -= editHeight;
	AddonsEdit->SetFrameGeometry(0.0, y, std::max(GetWidth() - browseWidth - clearWidth - 16.0, 0.0), editHeight);
	AddonsBrowseButton->SetFrameGeometry(GetWidth() - browseWidth - clearWidth - 8.0, y, browseWidth, editHeight);
	AddonsClearButton->SetFrameGeometry(GetWidth() - clearWidth, y, clearWidth, editHeight);

	const double addonsLabelHeight = AddonsLabel->GetPreferredHeight();
	y -= addonsLabelHeight;
	AddonsLabel->SetFrameGeometry(0.0, y, GetWidth(), addonsLabelHeight);

	y -= 20.0;
	GamesList->SetFrameGeometry(0.0, listViewTop, GetWidth(), std::max(y - listViewTop, 0.0));

	Launcher->UpdatePlayButton();

	GamesList->ScrollToItem(GamesList->GetSelectedItem());
}
