/*
** playgamepage.h
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

#pragma once

#include <zwidget/core/widget.h>
#include "tarray.h"
#include "zstring.h"

class LauncherWindow;
class TextLabel;
class ListView;
class LineEdit;
class CheckboxLabel;
class PushButton;
struct WadStuff;
struct FStartupSelectionInfo;

class PlayGamePage : public Widget
{
public:
	PlayGamePage(LauncherWindow* launcher, const FStartupSelectionInfo& info);
	void UpdateLanguage();
	void SetValues(FStartupSelectionInfo& info) const;

private:
	void OnGeometryChanged() override;
	void OnSetFocus() override;
	void OnGamesListActivated();
	void OnBrowseAddonsClicked();
	void OnClearAddonsClicked();
	bool OnFileDrop(std::string) override;
	void AddAddonFile(FString path);
	void SetAddonFiles(FString files);
	FString GetAddonFiles() const;
	void UpdateAddonFilesEdit();

	LauncherWindow* Launcher = nullptr;

	TextLabel* WelcomeLabel = nullptr;
	TextLabel* VersionLabel = nullptr;
	TextLabel* SelectLabel = nullptr;
	TextLabel* AddonsLabel = nullptr;
	TextLabel* ParametersLabel = nullptr;
	ListView* GamesList = nullptr;
	LineEdit* AddonsEdit = nullptr;
	PushButton* AddonsBrowseButton = nullptr;
	PushButton* AddonsClearButton = nullptr;
	LineEdit* ParametersEdit = nullptr;
	CheckboxLabel* SaveArgsCheckbox = nullptr;
	TArray<FString> AddonFiles;
};
