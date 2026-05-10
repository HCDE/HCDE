/*
** netstartwindow.cpp
**
**
**
**---------------------------------------------------------------------------
**
** Copyright 2024 Magnus Norddahl
** Copyright 2024-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

#include <zwidget/core/timer.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/textlabel/textlabel.h>

#include "basics.h"
#include "debugtrace.h"
#include "i_net.h"
#include "netstartwindow.h"

NetStartWindow* NetStartWindow::Instance = nullptr;

static FString BuildSessionSummaryText(const FServerQuerySnapshot& snapshot)
{
	FString info;
	info.Format("Server: %s | Host: %s | Map: %s | Players: %u/%u",
		snapshot.SessionState.GetChars(),
		snapshot.HostName.GetChars(),
		snapshot.MapName.GetChars(),
		snapshot.PlayerCount,
		snapshot.MaxPlayers);
	return info;
}

static FString BuildSessionStatusText(const FServerQuerySnapshot& snapshot)
{
	FString info;
	info.Format("Mode: %s | Skill: %u | Time left: %u | Frag limit: %u",
		snapshot.Deathmatch ? (snapshot.Teamplay ? "Deathmatch + Teamplay" : "Deathmatch") : (snapshot.Teamplay ? "Co-op + Teamplay" : "Co-op"),
		snapshot.Skill,
		snapshot.TimeLeft,
		snapshot.FragLimit);
	return info;
}

void NetStartWindow::NetInit(const char* message, bool host)
{
	DebugTrace::Markf("netui", "init host=%d", host ? 1 : 0);
	Size screenSize = GetScreenSize();
	double windowWidth = 450.0;
	double windowHeight = host ? 600.0 : 220.0;

	if (!Instance)
	{
		Instance = new NetStartWindow(host);
		Instance->SetFrameGeometry((screenSize.width - windowWidth) * 0.5, (screenSize.height - windowHeight) * 0.5, windowWidth, windowHeight);
		Instance->Show();
	}

	Instance->SetMessage(message);
	Instance->RefreshSessionSummary();
}

void NetStartWindow::NetMessage(const char* message)
{
	DebugTrace::Markf("netui", "message %s", message != nullptr ? message : "");
	if (Instance)
	{
		Instance->SetMessage(message);
		Instance->RefreshSessionSummary();
	}
}

void NetStartWindow::NetConnect(int client, const char* name, unsigned flags, int status)
{
	DebugTrace::Markf("netui", "connect client=%d status=%u flags=%u name=%s", client, status, flags, name != nullptr ? name : "");
	if (!Instance || Instance->RoomRoster == nullptr)
		return;

	std::string value = "";
	if (flags & 1)
		value.append("*");
	if (flags & 2)
		value.append("H");

	Instance->RoomRoster->UpdateItem(value, client, 1);
	Instance->RoomRoster->UpdateItem(name, client, 2);

	value = "";
	if (status == 1)
		value = "JOINING";
	else if (status == 2)
		value = "SYNCING";
	else if (status == 3)
		value = "READY";

	Instance->RoomRoster->UpdateItem(value, client, 3);
	if (Instance->hosting && client != 0 && status > 0 && !Instance->shouldstart)
	{
		DebugTrace::Markf("netui", "auto start session client=%d status=%d", client, status);
		Instance->shouldstart = true;
	}
	Instance->RefreshSessionSummary();
}

void NetStartWindow::NetUpdate(int client, int status)
{
	DebugTrace::Markf("netui", "update client=%d status=%d", client, status);
	if (!Instance || Instance->RoomRoster == nullptr)
		return;

	std::string value = "";
	if (status == 1)
		value = "JOINING";
	else if (status == 2)
		value = "SYNCING";
	else if (status == 3)
		value = "READY";

	Instance->RoomRoster->UpdateItem(value, client, 3);
	if (Instance->hosting && client != 0 && status > 0 && !Instance->shouldstart)
	{
		DebugTrace::Markf("netui", "auto start session client=%d status=%d", client, status);
		Instance->shouldstart = true;
	}
	Instance->RefreshSessionSummary();
}

void NetStartWindow::NetDisconnect(int client)
{
	DebugTrace::Markf("netui", "disconnect client=%d", client);
	if (Instance && Instance->RoomRoster != nullptr)
	{
		for (size_t i = 1u; i < Instance->RoomRoster->GetColumnAmount(); ++i)
			Instance->RoomRoster->UpdateItem("", client, int(i));
		Instance->RefreshSessionSummary();
	}
}

void NetStartWindow::NetProgress(int cur, int limit)
{
	DebugTrace::Markf("netui", "progress %d/%d", cur, limit);
	if (!Instance)
		return;

	Instance->maxpos = limit;
	Instance->SetProgress(cur);
	if (Instance->RoomRoster != nullptr)
	{
		for (int start = Instance->RoomRoster->GetItemAmount(); start < Instance->maxpos; ++start)
			Instance->RoomRoster->AddItem(std::to_string(start));
	}
	Instance->RefreshSessionSummary();
}

void NetStartWindow::NetDone()
{
	DebugTrace::Mark("netui", "done");
	delete Instance;
	Instance = nullptr;
}

void NetStartWindow::NetClose()
{
	DebugTrace::Mark("netui", "close");
	if (Instance != nullptr)
		Instance->OnClose();
}

bool NetStartWindow::ShouldStartNet()
{
	if (Instance != nullptr)
		return Instance->shouldstart;

	return false;
}

int NetStartWindow::GetNetKickClient()
{
	if (!Instance || !Instance->kickclients.size())
		return -1;

	int next = Instance->kickclients.back();
	Instance->kickclients.pop_back();
	return next;
}

int NetStartWindow::GetNetBanClient()
{
	if (!Instance || !Instance->banclients.size())
		return -1;

	int next = Instance->banclients.back();
	Instance->banclients.pop_back();
	return next;
}

bool NetStartWindow::NetLoop(bool (*loopCallback)(void*), void* data)
{
	if (!Instance)
		return false;

	Instance->timer_callback = loopCallback;
	Instance->userdata = data;
	Instance->CallbackException = {};

	DisplayWindow::RunLoop();

	Instance->timer_callback = nullptr;
	Instance->userdata = nullptr;

	if (Instance->CallbackException)
		std::rethrow_exception(Instance->CallbackException);

	return Instance->exitreason;
}

NetStartWindow::NetStartWindow(bool host) : Widget(nullptr, WidgetType::Window)
{
	SetWindowTitle(host ? "HCDE Host" : "HCDE Joining Server");

	MessageLabel = new TextLabel(this);
	ProgressLabel = new TextLabel(this);
	AbortButton = new PushButton(this);

	MessageLabel->SetTextAlignment(TextLabelAlignment::Center);
	ProgressLabel->SetTextAlignment(TextLabelAlignment::Center);

	AbortButton->OnClick = [this]() { OnClose(); };
	AbortButton->SetText(host ? "Stop Host" : "Cancel");

	if (host)
	{
		hosting = true;
		SessionLabel = new TextLabel(this);
		RoomRoster = new ListView(this);
		SessionLabel->SetTextAlignment(TextLabelAlignment::Center);

		ForceStartButton = new PushButton(this);
		ForceStartButton->OnClick = [this]() { ForceStart(); };
		ForceStartButton->SetText("Start Game");

		KickButton = new PushButton(this);
		KickButton->OnClick = [this]() { OnKick(); };
		KickButton->SetText("Remove");

		BanButton = new PushButton(this);
		BanButton->OnClick = [this]() { OnBan(); };
		BanButton->SetText("Block");

		// Slot number, flags, name, session status.
		RoomRoster->SetColumnWidths({ 30.0, 30.0, 200.0, 70.0 });
	}

	CallbackTimer = new Timer(this);
	CallbackTimer->FuncExpired = [this]() { OnCallbackTimerExpired(); };
	CallbackTimer->Start(500);
}

void NetStartWindow::SetMessage(const std::string& message)
{
	MessageLabel->SetText(message);
}

void NetStartWindow::SetProgress(int newpos)
{
	if (pos != newpos && maxpos > 1)
	{
		pos = newpos;
		FString message;
		message.Format("Players %d/%d", pos, maxpos);
		ProgressLabel->SetText(message.GetChars());
	}
}

void NetStartWindow::RefreshSessionSummary()
{
	if (!hosting || SessionLabel == nullptr || RoomRoster == nullptr)
		return;

	FServerQuerySnapshot snapshot = {};
	I_GetLocalServerSnapshot(snapshot);

	FString text = BuildSessionSummaryText(snapshot);
	text.AppendFormat("\n%s", BuildSessionStatusText(snapshot).GetChars());
	SessionLabel->SetText(text.GetChars());
}

void NetStartWindow::OnClose()
{
	DebugTrace::Mark("netui", "close requested");
	exitreason = false;
	DisplayWindow::ExitLoop();
}

void NetStartWindow::ForceStart()
{
	DebugTrace::Mark("netui", "start session");
	shouldstart = true;
}

void NetStartWindow::OnKick()
{
	int item = RoomRoster->GetSelectedItem();
	DebugTrace::Markf("netui", "remove client=%d", item);

	size_t i = 0u;
	for (; i < kickclients.size(); ++i)
	{
		if (kickclients[i] == item)
			break;
	}

	if (i >= kickclients.size())
		kickclients.push_back(item);
}

void NetStartWindow::OnBan()
{
	int item = RoomRoster->GetSelectedItem();
	DebugTrace::Markf("netui", "block client=%d", item);

	size_t i = 0u;
	for (; i < banclients.size(); ++i)
	{
		if (banclients[i] == item)
			break;
	}

	if (i >= banclients.size())
		banclients.push_back(item);
}

void NetStartWindow::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();

	double y = 15.0;
	double labelheight = MessageLabel->GetPreferredHeight();
	MessageLabel->SetFrameGeometry(Rect::xywh(5.0, y, w - 10.0, labelheight));
	y += labelheight;

	labelheight = ProgressLabel->GetPreferredHeight();
	ProgressLabel->SetFrameGeometry(Rect::xywh(5.0, y, w - 10.0, labelheight));
	y += labelheight + 5.0;

	if (!hosting || SessionLabel == nullptr || RoomRoster == nullptr)
	{
		AbortButton->SetFrameGeometry((w - 100.0) * 0.5, GetHeight() - 15.0 - AbortButton->GetPreferredHeight(), 100.0, AbortButton->GetPreferredHeight());
		return;
	}

	labelheight = SessionLabel->GetPreferredHeight();
	SessionLabel->SetFrameGeometry(Rect::xywh(5.0, y, w - 10.0, labelheight));
	y += labelheight + 5.0;

	labelheight = (GetHeight() - 30.0 - AbortButton->GetPreferredHeight()) - y;
	RoomRoster->SetFrameGeometry(Rect::xywh(5.0, y, w - 10.0, labelheight));

	y = GetHeight() - 15.0 - AbortButton->GetPreferredHeight();
	if (hosting)
	{
		Widget *bs[] = {AbortButton, BanButton, KickButton, ForceStartButton};
		const size_t n = sizeof(bs)/sizeof(bs[0]);
		double ws[n];
		double hs[n];
		double pos = 0, padding = 10.0;
		for (size_t i = 0; i < n; i++)
		{
			ws[i] = bs[i]->GetPreferredWidth();
			hs[i] = bs[i]->GetPreferredHeight();
			pos += ws[i] + padding;
		}
		pos = (w - pos + padding) / 2;
		for (size_t i = 0; i < n; i++)
		{
			bs[i]->SetFrameGeometry(pos, y, ws[i], hs[i]);
			pos += ws[i] + padding;
		}
	}
	else
	{
		AbortButton->SetFrameGeometry((w - 100.0) * 0.5, y, 100.0, AbortButton->GetPreferredHeight());
	}
}

void NetStartWindow::OnCallbackTimerExpired()
{
	if (timer_callback)
	{
		bool result = false;
		try
		{
			result = timer_callback(userdata);
		}
		catch (...)
		{
			CallbackException = std::current_exception();
		}

		if (result)
		{
			exitreason = true;
			DisplayWindow::ExitLoop();
		}
	}
}
