/*
** aboutpage.h
**
** About tab of launcher
**
**---------------------------------------------------------------------------
**
** Copyright 2025 Marcus Minhorst
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

#pragma once

#include <zwidget/core/widget.h>

class LauncherWindow;
class TextEdit;
class TextLabel;
class PushButton;
struct FStartupSelectionInfo;

class AboutPage : public Widget
{
public:
	AboutPage(LauncherWindow* launcher, const FStartupSelectionInfo& info);
	void UpdateLanguage();
	void SetValues(FStartupSelectionInfo& info) const;

private:
	void OnGeometryChanged() override;
#ifdef _WIN32
	void OnCheckUpdatesClicked();
	void OnInstallUpdateClicked();
	void OnOpenUpdateLogsClicked();
	void OnOpenLastUpdateLogClicked();
	void SetUpdateActionBusy(bool busy);
	void UpdateInstallButtonState();
#endif

	LauncherWindow* Launcher = nullptr;

	TextEdit* Text = nullptr;
	PushButton* Notes = nullptr;
#ifdef _WIN32
	TextLabel* UpdateStatus = nullptr;
	PushButton* CheckUpdates = nullptr;
	PushButton* InstallUpdate = nullptr;
	PushButton* OpenUpdateLogs = nullptr;
	PushButton* OpenLastUpdateLog = nullptr;
	FString PendingUpdateVersion = {};
	FString PendingUpdateUrl = {};
	FString PendingAssetName = {};
	bool AutoCheckOnStartup = true;
	bool UpdateActionBusy = false;
	bool UpdateLockActive = false;
#endif
};
