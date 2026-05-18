/*
** launcherbuttonbar.cpp
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

#include <algorithm>

#include <zwidget/widgets/pushbutton/pushbutton.h>

#include "gstrings.h"
#include "launcherbuttonbar.h"
#include "launcherwindow.h"

LauncherButtonbar::LauncherButtonbar(LauncherWindow* parent) : Widget(parent)
{
	PlayButton = new PushButton(this);
	ExitButton = new PushButton(this);
	UpdateButton = new PushButton(this);

	PlayButton->OnClick = [this]() { OnPlayButtonClicked(); };
	ExitButton->OnClick = [this]() { OnExitButtonClicked(); };
	UpdateButton->OnClick = [this]() { OnUpdateButtonClicked(); };
	UpdateButton->SetVisible(false);
}

void LauncherButtonbar::UpdateLanguage()
{
	auto launcher = GetLauncher();
	if (!launcher->IsInMultiplayer())
		PlayButton->SetText(GStrings.GetString("PICKER_PLAY"));
	else if (launcher->IsHosting())
		PlayButton->SetText(GStrings.GetString("PICKER_PLAYHOST"));
	else
		PlayButton->SetText(GStrings.GetString("PICKER_PLAYJOIN"));

	ExitButton->SetText(GStrings.GetString("PICKER_EXIT"));
}

void LauncherButtonbar::SetUpdateNotice(const FString& text, bool visible)
{
	const bool hasText = visible && text.IsNotEmpty();
	UpdateButton->SetVisible(hasText);
	UpdateButton->SetEnabled(hasText);
	if (hasText)
	{
		UpdateButton->SetText(text.GetChars());
	}
	OnGeometryChanged();
}

double LauncherButtonbar::GetPreferredHeight()
{
	return 20.0 + std::max(PlayButton->GetPreferredHeight(), ExitButton->GetPreferredHeight());
}

void LauncherButtonbar::OnGeometryChanged()
{
	double w, h;
	h = std::max(PlayButton->GetPreferredHeight(), ExitButton->GetPreferredHeight());
	w = 10 + std::max(PlayButton->GetPreferredWidth(), ExitButton->GetPreferredWidth());
	PlayButton->SetFrameGeometry(20.0, 10.0, w, h);
	ExitButton->SetFrameGeometry(GetWidth() - 20.0 - w, 10.0, w, h);
	if (UpdateButton->IsVisible())
	{
		const double updateWidth = std::min<double>(GetWidth() * 0.45, 20.0 + UpdateButton->GetPreferredWidth());
		UpdateButton->SetFrameGeometry((GetWidth() - updateWidth) * 0.5, 10.0, updateWidth, h);
	}
	else
	{
		UpdateButton->SetFrameGeometry(0.0, 0.0, 0.0, 0.0);
	}
}

void LauncherButtonbar::OnPlayButtonClicked()
{
	GetLauncher()->Start();
}

void LauncherButtonbar::OnExitButtonClicked()
{
	GetLauncher()->Exit();
}

void LauncherButtonbar::OnUpdateButtonClicked()
{
	GetLauncher()->OpenAboutPage();
}

LauncherWindow* LauncherButtonbar::GetLauncher() const
{
	return static_cast<LauncherWindow*>(Parent());
}
