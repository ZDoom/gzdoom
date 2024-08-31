
#include "launcherbuttonbar.h"
#include "launcherwindow.h"
#include "gstrings.h"
#include <zwidget/widgets/pushbutton/pushbutton.h>

LauncherButtonbar::LauncherButtonbar(LauncherWindow* parent) : Widget(parent)
{
	PlayButton = new PushButton(this);
	ExitButton = new PushButton(this);

	PlayButton->OnClick = [=]() { OnPlayButtonClicked(); };
	ExitButton->OnClick = [=]() { OnExitButtonClicked(); };
}

void LauncherButtonbar::UpdateLanguage()
{
	PlayButton->SetText(GStrings.GetString("PICKER_PLAY"));
	ExitButton->SetText(GStrings.GetString("PICKER_EXIT"));
}

double LauncherButtonbar::GetPreferredHeight() const
{
	return 20.0 + std::max(PlayButton->GetPreferredHeight(), ExitButton->GetPreferredHeight());
}

void LauncherButtonbar::OnGeometryChanged()
{
	PlayButton->SetFrameGeometry(20.0, 10.0, 120.0, PlayButton->GetPreferredHeight());
	ExitButton->SetFrameGeometry(GetWidth() - 20.0 - 120.0, 10.0, 120.0, PlayButton->GetPreferredHeight());
}

void LauncherButtonbar::OnPlayButtonClicked()
{
	GetLauncher()->Start();
}

void LauncherButtonbar::OnExitButtonClicked()
{
	GetLauncher()->Exit();
}

LauncherWindow* LauncherButtonbar::GetLauncher() const
{
	return static_cast<LauncherWindow*>(Parent());
}
