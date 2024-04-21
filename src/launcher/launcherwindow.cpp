#include "launcherwindow.h"
#include "launcherbanner.h"
#include "launcherbuttonbar.h"
#include "playgamepage.h"
#include "settingspage.h"
#include "v_video.h"
#include "version.h"
#include "i_interface.h"
#include "gstrings.h"
#include <zwidget/core/resourcedata.h>
#include <zwidget/window/window.h>
#include <zwidget/widgets/tabwidget/tabwidget.h>

int LauncherWindow::ExecModal(WadStuff* wads, int numwads, int defaultiwad, int* autoloadflags)
{
	Size screenSize = GetScreenSize();
	double windowWidth = 615.0;
	double windowHeight = 700.0;

	auto launcher = std::make_unique<LauncherWindow>(wads, numwads, defaultiwad, autoloadflags);
	launcher->SetFrameGeometry((screenSize.width - windowWidth) * 0.5, (screenSize.height - windowHeight) * 0.5, windowWidth, windowHeight);
	launcher->Show();

	DisplayWindow::RunLoop();

	return launcher->ExecResult;
}

LauncherWindow::LauncherWindow(WadStuff* wads, int numwads, int defaultiwad, int* autoloadflags) : Widget(nullptr, WidgetType::Window)
{
	SetWindowBackground(Colorf::fromRgba8(51, 51, 51));
	SetWindowBorderColor(Colorf::fromRgba8(51, 51, 51));
	SetWindowCaptionColor(Colorf::fromRgba8(33, 33, 33));
	SetWindowCaptionTextColor(Colorf::fromRgba8(226, 223, 219));
	SetWindowTitle(GAMENAME);

	Banner = new LauncherBanner(this);
	Pages = new TabWidget(this);
	Buttonbar = new LauncherButtonbar(this);

	PlayGame = new PlayGamePage(this, wads, numwads, defaultiwad);
	Settings = new SettingsPage(this, autoloadflags);

	Pages->AddTab(PlayGame, "Play");
	Pages->AddTab(Settings, "Settings");

	UpdateLanguage();

	Pages->SetCurrentWidget(PlayGame);
	PlayGame->SetFocus();
}

void LauncherWindow::Start()
{
	Settings->Save();

	std::string extraargs = PlayGame->GetExtraArgs();
	if (!extraargs.empty())
	{
		// To do: restart the process like the cocoa backend is doing?
	}

	ExecResult = PlayGame->GetSelectedGame();
	DisplayWindow::ExitLoop();
}

void LauncherWindow::Exit()
{
	ExecResult = -1;
	DisplayWindow::ExitLoop();
}

void LauncherWindow::UpdateLanguage()
{
	Pages->SetTabText(PlayGame, GStrings.GetString("PICKER_TAB_PLAY"));
	Pages->SetTabText(Settings, GStrings.GetString("OPTMNU_TITLE"));
	Banner->UpdateLanguage();
	PlayGame->UpdateLanguage();
	Settings->UpdateLanguage();
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
