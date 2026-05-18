/*
** launcherbuttonbar.h
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

#include "zstring.h"

class LauncherWindow;
class PushButton;

class LauncherButtonbar : public Widget
{
public:
	LauncherButtonbar(LauncherWindow* parent);
	void UpdateLanguage();
	void SetUpdateNotice(const FString& text, bool visible);

	double GetPreferredHeight() override;

private:
	void OnGeometryChanged() override;
	void OnPlayButtonClicked();
	void OnExitButtonClicked();
	void OnUpdateButtonClicked();

	LauncherWindow* GetLauncher() const;

	PushButton* PlayButton = nullptr;
	PushButton* ExitButton = nullptr;
	PushButton* UpdateButton = nullptr;
};
