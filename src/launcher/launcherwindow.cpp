/*
** launcherwindow.cpp
**
**---------------------------------------------------------------------------
**
** Copyright 2024-2025 GZDoom Maintainers and Contributors
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/
**
**---------------------------------------------------------------------------
**
*/

#include <zwidget/core/resourcedata.h>
#include <zwidget/widgets/tabwidget/tabwidget.h>
#include <zwidget/window/window.h>

#include "aboutpage.h"
#include "gstrings.h"
#include "i_interface.h"
#include "launcherbanner.h"
#include "launcherbuttonbar.h"
#include "launcherwindow.h"
#include "networkpage.h"
#include "playgamepage.h"
#include "releasepage.h"
#include "settingspage.h"
#include "version.h"

bool LauncherWindow::ExecModal(FStartupSelectionInfo& info)
{
	Size screenSize = GetScreenSize();
	double windowWidth = 615.0;
	double windowHeight = 700.0;

	auto launcher = std::make_unique<LauncherWindow>(info);
	launcher->SetFrameGeometry((screenSize.width - windowWidth) * 0.5, (screenSize.height - windowHeight) * 0.5, windowWidth, windowHeight);
	launcher->Show();

	DisplayWindow::RunLoop();

	return launcher->ExecResult;
}

LauncherWindow::LauncherWindow(FStartupSelectionInfo& info) : Widget(nullptr, WidgetType::Window), Info(&info)
{
	SetWindowTitle(GAMENAME);

	Banner = new LauncherBanner(this);
	Pages = new TabWidget(this);
	Buttonbar = new LauncherButtonbar(this);

	bool releasenotes = info.isNewRelease && info.notifyNewRelease;

	PlayGame = new PlayGamePage(this, info);
	Settings = new SettingsPage(this, info);
	Network = new NetworkPage(this, info);
	About = new AboutPage(this, info);

	if (releasenotes)
	{
		Release = new ReleasePage(this, info);
		Pages->AddTab(Release, "Release Notes");
	}

	Pages->AddTab(PlayGame, "Play");
	Pages->AddTab(Settings, "Settings");
	Pages->AddTab(Network, "Multiplayer");
	Pages->AddTab(About, "About");

	Network->InitializeTabs(info);

	UpdateLanguage();

	Pages->SetCurrentIndex(0);
	Pages->GetCurrentWidget()->SetFocus();
}

void LauncherWindow::UpdatePlayButton()
{
	Buttonbar->UpdateLanguage();
}

bool LauncherWindow::IsInMultiplayer() const
{
	return Pages->GetCurrentIndex() >= 0 ? Pages->GetCurrentWidget() == Network : false;
}

bool LauncherWindow::IsHosting() const
{
	return IsInMultiplayer() && Network->IsInHost();
}

void LauncherWindow::Start()
{
	Info->bNetStart = IsInMultiplayer();

	Settings->SetValues(*Info);
	if (Info->bNetStart)
		Network->SetValues(*Info);
	else
		PlayGame->SetValues(*Info);

	if (Release)
		Release->SetValues(*Info);

	ExecResult = true;
	DisplayWindow::ExitLoop();
}

void LauncherWindow::Exit()
{
	ExecResult = false;
	DisplayWindow::ExitLoop();
}

void LauncherWindow::UpdateLanguage()
{
	Pages->SetTabText(PlayGame, GStrings.GetString("PICKER_TAB_PLAY"));
	Pages->SetTabText(Settings, GStrings.GetString("OPTMNU_TITLE"));
	Pages->SetTabText(Network, GStrings.GetString("PICKER_TAB_MULTI"));
	Pages->SetTabText(About, GStrings.GetString("PICKER_TAB_ABOUT"));
	PlayGame->UpdateLanguage();
	Settings->UpdateLanguage();
	Network->UpdateLanguage();
	About->UpdateLanguage();
	if (Release)
	{
		Pages->SetTabText(Release, GStrings.GetString("PICKER_TAB_RELEASE"));
		Release->UpdateLanguage();
	}
	Buttonbar->UpdateLanguage();
}

void LauncherWindow::OnClose()
{
	Exit();
}

void LauncherWindow::OnGeometryChanged()
{
	double top = 0.0;
	double bottom = GetHeight();

	Banner->SetFrameGeometry(0.0, top, GetWidth(), Banner->GetPreferredHeight());
	top += Banner->GetPreferredHeight();

	bottom -= Buttonbar->GetPreferredHeight();
	Buttonbar->SetFrameGeometry(0.0, bottom, GetWidth(), Buttonbar->GetPreferredHeight());

	Pages->SetFrameGeometry(0.0, top, GetWidth(), std::max(bottom - top, 0.0));
}
