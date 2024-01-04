#include "launcherwindow.h"
#include "v_video.h"
#include "version.h"
#include "i_interface.h"
#include <zwidget/core/image.h>
#include <zwidget/window/window.h>
#include <zwidget/widgets/textedit/textedit.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/imagebox/imagebox.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include <zwidget/widgets/lineedit/lineedit.h>

#ifdef RENDER_BACKENDS
EXTERN_CVAR(Int, vid_preferbackend);
#endif

EXTERN_CVAR(Bool, queryiwad);

int LauncherWindow::ExecModal(WadStuff* wads, int numwads, int defaultiwad, int* autoloadflags)
{
	Size screenSize = GetScreenSize();
	double windowWidth = 615.0;
	double windowHeight = 668.0;

	auto launcher = std::make_unique<LauncherWindow>(wads, numwads, defaultiwad, autoloadflags);
	launcher->SetFrameGeometry((screenSize.width - windowWidth) * 0.5, (screenSize.height - windowHeight) * 0.5, windowWidth, windowHeight);
	launcher->Show();

	DisplayWindow::RunLoop();

	return launcher->ExecResult;
}

LauncherWindow::LauncherWindow(WadStuff* wads, int numwads, int defaultiwad, int* autoloadflags) : Widget(nullptr, WidgetType::Window), AutoloadFlags(autoloadflags)
{
	SetWindowBackground(Colorf::fromRgba8(51, 51, 51));
	SetWindowBorderColor(Colorf::fromRgba8(51, 51, 51));
	SetWindowCaptionColor(Colorf::fromRgba8(33, 33, 33));
	SetWindowCaptionTextColor(Colorf::fromRgba8(226, 223, 219));
	SetWindowTitle(GAMENAME);

	Logo = new ImageBox(this);
	WelcomeLabel = new TextLabel(this);
	VersionLabel = new TextLabel(this);
	SelectLabel = new TextLabel(this);
	GeneralLabel = new TextLabel(this);
	ExtrasLabel = new TextLabel(this);
	ParametersLabel = new TextLabel(this);
	FullscreenCheckbox = new CheckboxLabel(this);
	DisableAutoloadCheckbox = new CheckboxLabel(this);
	DontAskAgainCheckbox = new CheckboxLabel(this);
	LightsCheckbox = new CheckboxLabel(this);
	BrightmapsCheckbox = new CheckboxLabel(this);
	WidescreenCheckbox = new CheckboxLabel(this);
	PlayButton = new PushButton(this);
	ExitButton = new PushButton(this);
	GamesList = new ListView(this);
	ParametersEdit = new LineEdit(this);

	PlayButton->OnClick = [=]() { OnPlayButtonClicked(); };
	ExitButton->OnClick = [=]() { OnExitButtonClicked(); };
	GamesList->OnActivated = [=]() { OnGamesListActivated(); };

	SelectLabel->SetText("Select which game file (IWAD) to run.");
	PlayButton->SetText("Play Game");
	ExitButton->SetText("Exit");

	GeneralLabel->SetText("General");
	ExtrasLabel->SetText("Extra Graphics");
	FullscreenCheckbox->SetText("Fullscreen");
	DisableAutoloadCheckbox->SetText("Disable autoload");
	DontAskAgainCheckbox->SetText("Don't ask me again");
	LightsCheckbox->SetText("Lights");
	BrightmapsCheckbox->SetText("Brightmaps");
	WidescreenCheckbox->SetText("Widescreen");
	ParametersLabel->SetText("Additional Parameters:");

#ifdef RENDER_BACKENDS
	BackendLabel = new TextLabel(this);
	VulkanCheckbox = new CheckboxLabel(this);
	OpenGLCheckbox = new CheckboxLabel(this);
	GLESCheckbox = new CheckboxLabel(this);
	BackendLabel->SetText("Render Backend");
	VulkanCheckbox->SetText("Vulkan");
	OpenGLCheckbox->SetText("OpenGL");
	GLESCheckbox->SetText("OpenGL ES");
#endif

	FString welcomeText, versionText;
	welcomeText.Format("Welcome to %s!", GAMENAME);
	versionText.Format("Version %s.", GetVersionString());
	WelcomeLabel->SetText(welcomeText.GetChars());
	VersionLabel->SetText(versionText.GetChars());

	FullscreenCheckbox->SetChecked(vid_fullscreen);
	DontAskAgainCheckbox->SetChecked(!queryiwad);

	int flags = *autoloadflags;
	DisableAutoloadCheckbox->SetChecked(flags & 1);
	LightsCheckbox->SetChecked(flags & 2);
	BrightmapsCheckbox->SetChecked(flags & 4);
	WidescreenCheckbox->SetChecked(flags & 8);

#ifdef RENDER_BACKENDS
	OpenGLCheckbox->SetRadioStyle(true);
	VulkanCheckbox->SetRadioStyle(true);
	GLESCheckbox->SetRadioStyle(true);
	OpenGLCheckbox->FuncChanged = [this](bool on) { if (on) { VulkanCheckbox->SetChecked(false); GLESCheckbox->SetChecked(false); }};
	VulkanCheckbox->FuncChanged = [this](bool on) { if (on) { OpenGLCheckbox->SetChecked(false); GLESCheckbox->SetChecked(false); }};
	GLESCheckbox->FuncChanged = [this](bool on) { if (on) { VulkanCheckbox->SetChecked(false); OpenGLCheckbox->SetChecked(false); }};
	switch (vid_preferbackend)
	{
	case 0:
		OpenGLCheckbox->SetChecked(true);
		break;
	case 1:
		VulkanCheckbox->SetChecked(true);
		break;
	case 2:
		GLESCheckbox->SetChecked(true);
		break;
	}
#endif

	for (int i = 0; i < numwads; i++)
	{
		const char* filepart = strrchr(wads[i].Path.GetChars(), '/');
		if (filepart == NULL)
			filepart = wads[i].Path.GetChars();
		else
			filepart++;

		FString work;
		if (*filepart) work.Format("%s (%s)", wads[i].Name.GetChars(), filepart);
		else work = wads[i].Name.GetChars();

		GamesList->AddItem(work.GetChars());
	}

	Logo->SetImage(Image::LoadResource("widgets/banner.png"));

	GamesList->SetFocus();
}

void LauncherWindow::OnClose()
{
	OnExitButtonClicked();
}

void LauncherWindow::OnPlayButtonClicked()
{
	vid_fullscreen = FullscreenCheckbox->GetChecked();
	queryiwad = !DontAskAgainCheckbox->GetChecked();

	int flags = 0;
	if (DisableAutoloadCheckbox->GetChecked()) flags |= 1;
	if (LightsCheckbox->GetChecked()) flags |= 2;
	if (BrightmapsCheckbox->GetChecked()) flags |= 4;
	if (WidescreenCheckbox->GetChecked()) flags |= 8;
	*AutoloadFlags = flags;

#ifdef RENDER_BACKENDS
	int v = 1;
	if (OpenGLCheckbox->GetChecked()) v = 0;
	else if (VulkanCheckbox->GetChecked()) v = 1;
	else if (GLESCheckbox->GetChecked()) v = 2;
	if (v != vid_preferbackend) vid_preferbackend = v;
#endif

	std::string extraargs = ParametersEdit->GetText();
	if (!extraargs.empty())
	{
		// To do: restart the process like the cocoa backend is doing?
	}

	ExecResult = GamesList->GetSelectedItem();
	DisplayWindow::ExitLoop();
}

void LauncherWindow::OnExitButtonClicked()
{
	ExecResult = -1;
	DisplayWindow::ExitLoop();
}

void LauncherWindow::OnGamesListActivated()
{
	OnPlayButtonClicked();
}

void LauncherWindow::OnGeometryChanged()
{
	double y = 0.0;

	Logo->SetFrameGeometry(0.0, y, GetWidth(), Logo->GetPreferredHeight());
	y += Logo->GetPreferredHeight();

	y += 10.0;

	WelcomeLabel->SetFrameGeometry(20.0, y, GetWidth() - 40.0, WelcomeLabel->GetPreferredHeight());
	y += WelcomeLabel->GetPreferredHeight();

	VersionLabel->SetFrameGeometry(20.0, y, GetWidth() - 40.0, VersionLabel->GetPreferredHeight());
	y += VersionLabel->GetPreferredHeight();

	y += 10.0;

	SelectLabel->SetFrameGeometry(20.0, y, GetWidth() - 40.0, SelectLabel->GetPreferredHeight());
	y += SelectLabel->GetPreferredHeight();

	double listViewTop = y + 10.0;

	y = GetHeight() - 15.0 - PlayButton->GetPreferredHeight();
	PlayButton->SetFrameGeometry(20.0, y, 120.0, PlayButton->GetPreferredHeight());
	ExitButton->SetFrameGeometry(GetWidth() - 20.0 - 120.0, y, 120.0, PlayButton->GetPreferredHeight());

	y -= 20.0;

	double editHeight = 24.0;
	y -= editHeight;
	ParametersEdit->SetFrameGeometry(20.0, y, GetWidth() - 40.0, editHeight);
	y -= 5.0;

	double labelHeight = ParametersLabel->GetPreferredHeight();
	y -= labelHeight;
	ParametersLabel->SetFrameGeometry(20.0, y, GetWidth() - 40.0, labelHeight);
	y -= 10.0;

	double panelWidth = 150.0;

#ifdef RENDER_BACKENDS
	auto yy = y;
	y -= GLESCheckbox->GetPreferredHeight();
	double x = GetWidth() / 2 - panelWidth / 2;
	GLESCheckbox->SetFrameGeometry(x, y, 190.0, GLESCheckbox->GetPreferredHeight());

	y -= OpenGLCheckbox->GetPreferredHeight();
	OpenGLCheckbox->SetFrameGeometry(x, y, 190.0, OpenGLCheckbox->GetPreferredHeight());

	y -= VulkanCheckbox->GetPreferredHeight();
	VulkanCheckbox->SetFrameGeometry(x, y, 190.0, VulkanCheckbox->GetPreferredHeight());

	y -= BackendLabel->GetPreferredHeight();
	BackendLabel->SetFrameGeometry(x, y, 190.0, BackendLabel->GetPreferredHeight());

	y = yy;
#endif
	y -= DontAskAgainCheckbox->GetPreferredHeight();
	DontAskAgainCheckbox->SetFrameGeometry(20.0, y, 190.0, DontAskAgainCheckbox->GetPreferredHeight());
	WidescreenCheckbox->SetFrameGeometry(GetWidth() - 20.0 - panelWidth, y, panelWidth, WidescreenCheckbox->GetPreferredHeight());

	y -= DisableAutoloadCheckbox->GetPreferredHeight();
	DisableAutoloadCheckbox->SetFrameGeometry(20.0, y, 190.0, DisableAutoloadCheckbox->GetPreferredHeight());
	BrightmapsCheckbox->SetFrameGeometry(GetWidth() - 20.0 - panelWidth, y, panelWidth, BrightmapsCheckbox->GetPreferredHeight());

	y -= FullscreenCheckbox->GetPreferredHeight();
	FullscreenCheckbox->SetFrameGeometry(20.0, y, 190.0, FullscreenCheckbox->GetPreferredHeight());
	LightsCheckbox->SetFrameGeometry(GetWidth() - 20.0 - panelWidth, y, panelWidth, LightsCheckbox->GetPreferredHeight());

	y -= GeneralLabel->GetPreferredHeight();
	GeneralLabel->SetFrameGeometry(20.0, y, 190.0, GeneralLabel->GetPreferredHeight());
	ExtrasLabel->SetFrameGeometry(GetWidth() - 20.0 - panelWidth, y, panelWidth, ExtrasLabel->GetPreferredHeight());

	double listViewBottom = y - 10.0;
	GamesList->SetFrameGeometry(20.0, listViewTop, GetWidth() - 40.0, std::max(listViewBottom - listViewTop, 0.0));
}
