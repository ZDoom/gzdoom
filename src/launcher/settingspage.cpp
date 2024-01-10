
#include "settingspage.h"
#include "launcherwindow.h"
#include "gstrings.h"
#include "i_interface.h"
#include "v_video.h"
#include <zwidget/core/resourcedata.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>

#ifdef RENDER_BACKENDS
EXTERN_CVAR(Int, vid_preferbackend);
#endif

EXTERN_CVAR(String, language)
EXTERN_CVAR(Bool, queryiwad);

SettingsPage::SettingsPage(LauncherWindow* launcher, int* autoloadflags) : Widget(nullptr), Launcher(launcher), AutoloadFlags(autoloadflags)
{
	LangLabel = new TextLabel(this);
	GeneralLabel = new TextLabel(this);
	ExtrasLabel = new TextLabel(this);
	FullscreenCheckbox = new CheckboxLabel(this);
	DisableAutoloadCheckbox = new CheckboxLabel(this);
	DontAskAgainCheckbox = new CheckboxLabel(this);
	LightsCheckbox = new CheckboxLabel(this);
	BrightmapsCheckbox = new CheckboxLabel(this);
	WidescreenCheckbox = new CheckboxLabel(this);

#ifdef RENDER_BACKENDS
	BackendLabel = new TextLabel(this);
	VulkanCheckbox = new CheckboxLabel(this);
	OpenGLCheckbox = new CheckboxLabel(this);
	GLESCheckbox = new CheckboxLabel(this);
#endif

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

	LangList = new ListView(this);

	try
	{
		auto data = LoadWidgetData("menudef.txt");
		FScanner sc;
		sc.OpenMem("menudef.txt", data);
		while (sc.GetString())
		{
			if (sc.Compare("OptionString"))
			{
				sc.MustGetString();
				if (sc.Compare("LanguageOptions"))
				{
					sc.MustGetStringName("{");
					while (!sc.CheckString("}"))
					{
						sc.MustGetString();
						FString iso = sc.String;
						sc.MustGetStringName(",");
						sc.MustGetString();
						if(iso.CompareNoCase("auto"))
							languages.push_back(std::make_pair(iso, FString(sc.String)));
					}
				}
			}
		}
	}
	catch (const std::exception& ex)
	{
		hideLanguage = true;
	}
	int i = 0;
	for (auto& l : languages)
	{
		LangList->AddItem(l.second.GetChars());
		if (!l.first.CompareNoCase(::language))
			LangList->SetSelectedItem(i);
		i++;
	}

	LangList->OnChanged = [=](int i) { OnLanguageChanged(i); };
}

void SettingsPage::Save()
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
}

void SettingsPage::UpdateLanguage()
{
	LangLabel->SetText(GStrings("OPTMNU_LANGUAGE"));
	GeneralLabel->SetText(GStrings("PICKER_GENERAL"));
	ExtrasLabel->SetText(GStrings("PICKER_EXTRA"));
	FullscreenCheckbox->SetText(GStrings("PICKER_FULLSCREEN"));
	DisableAutoloadCheckbox->SetText(GStrings("PICKER_NOAUTOLOAD"));
	DontAskAgainCheckbox->SetText(GStrings("PICKER_DONTASK"));
	LightsCheckbox->SetText(GStrings("PICKER_LIGHTS"));
	BrightmapsCheckbox->SetText(GStrings("PICKER_BRIGHTMAPS"));
	WidescreenCheckbox->SetText(GStrings("PICKER_WIDESCREEN"));

#ifdef RENDER_BACKENDS
	BackendLabel->SetText(GStrings("PICKER_PREFERBACKEND"));
	VulkanCheckbox->SetText(GStrings("OPTVAL_VULKAN"));
	OpenGLCheckbox->SetText(GStrings("OPTVAL_OPENGL"));
	GLESCheckbox->SetText(GStrings("OPTVAL_OPENGLES"));
#endif
}

void SettingsPage::OnLanguageChanged(int i)
{
	::language = languages[i].first.GetChars();
	GStrings.UpdateLanguage(::language); // CVAR callbacks are not active yet.
	UpdateLanguage();
	Update();
	Launcher->UpdateLanguage();
}

void SettingsPage::OnGeometryChanged()
{
	double panelWidth = 200.0;
	double y = 0.0;
	double w = GetWidth();
	double h = GetHeight();

	GeneralLabel->SetFrameGeometry(0.0, y, 190.0, GeneralLabel->GetPreferredHeight());
	ExtrasLabel->SetFrameGeometry(w - panelWidth, y, panelWidth, ExtrasLabel->GetPreferredHeight());
	y += GeneralLabel->GetPreferredHeight();

	FullscreenCheckbox->SetFrameGeometry(0.0, y, 190.0, FullscreenCheckbox->GetPreferredHeight());
	LightsCheckbox->SetFrameGeometry(w - panelWidth, y, panelWidth, LightsCheckbox->GetPreferredHeight());
	y += FullscreenCheckbox->GetPreferredHeight();

	DisableAutoloadCheckbox->SetFrameGeometry(0.0, y, 190.0, DisableAutoloadCheckbox->GetPreferredHeight());
	BrightmapsCheckbox->SetFrameGeometry(w - panelWidth, y, panelWidth, BrightmapsCheckbox->GetPreferredHeight());
	y += DisableAutoloadCheckbox->GetPreferredHeight();

	DontAskAgainCheckbox->SetFrameGeometry(0.0, y, 190.0, DontAskAgainCheckbox->GetPreferredHeight());
	WidescreenCheckbox->SetFrameGeometry(w - panelWidth, y, panelWidth, WidescreenCheckbox->GetPreferredHeight());
	y += DontAskAgainCheckbox->GetPreferredHeight();

#ifdef RENDER_BACKENDS
	double x = w / 2 - panelWidth / 2;
	y = 0;
	BackendLabel->SetFrameGeometry(x, y, 190.0, BackendLabel->GetPreferredHeight());
	y += BackendLabel->GetPreferredHeight();

	VulkanCheckbox->SetFrameGeometry(x, y, 190.0, VulkanCheckbox->GetPreferredHeight());
	y += VulkanCheckbox->GetPreferredHeight();

	OpenGLCheckbox->SetFrameGeometry(x, y, 190.0, OpenGLCheckbox->GetPreferredHeight());
	y += OpenGLCheckbox->GetPreferredHeight();

	GLESCheckbox->SetFrameGeometry(x, y, 190.0, GLESCheckbox->GetPreferredHeight());
	y += GLESCheckbox->GetPreferredHeight();
#endif

	if (!hideLanguage)
	{
		LangLabel->SetFrameGeometry(0.0, y, w, LangLabel->GetPreferredHeight());
		y += LangLabel->GetPreferredHeight();
		LangList->SetFrameGeometry(0.0, y, w, std::max(h - y, 0.0));
	}
}
