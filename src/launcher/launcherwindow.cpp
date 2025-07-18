#include "launcherwindow.h"
#include "launcherbanner.h"
#include "launcherbuttonbar.h"
#include "playgamepage.h"
#include "settingspage.h"
#include "networkpage.h"
#include "v_video.h"
#include "version.h"
#include "i_interface.h"
#include "gstrings.h"
#include "c_cvars.h"
#include <zwidget/core/resourcedata.h>
#include <zwidget/window/window.h>
#include <zwidget/widgets/tabwidget/tabwidget.h>

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

	PlayGame = new PlayGamePage(this, info);
	Settings = new SettingsPage(this, info);
	Network = new NetworkPage(this, info);

	Pages->AddTab(PlayGame, "Play");
	Pages->AddTab(Settings, "Settings");
	Pages->AddTab(Network, "Multiplayer");

	Network->InitializeTabs(info);

	UpdateLanguage();

	Pages->SetCurrentWidget(PlayGame);
	PlayGame->SetFocus();
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
	PlayGame->UpdateLanguage();
	Settings->UpdateLanguage();
	Network->UpdateLanguage();
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
