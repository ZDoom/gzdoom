/*
** settingspage.cpp
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

#include "settingspage.h"
#include "findfile.h"
#include "launcherwindow.h"
#include "gstrings.h"
#include "i_interface.h"
#include "sc_man.h"
#include "zwidget/widgets/dropdown/dropdown.h"
#include <zwidget/core/resourcedata.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/dropdown/dropdown.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>

static constexpr struct { const char* string; int flag; } FILELOAD_OPTS[] = {
	{"OPTVAL_LAX", REQUIRE_NONE},
	{"OPTVAL_DEFAULT", REQUIRE_DEFAULT},
	{"OPTVAL_STRICT", REQUIRE_ALL},
	{"OPTVAL_CUSTOM", -1}
};

SettingsPage::SettingsPage(LauncherWindow* launcher, const FStartupSelectionInfo& info) : Widget(nullptr), Launcher(launcher)
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
	SupportWadsCheckbox = new CheckboxLabel(this);

	FullscreenCheckbox->SetChecked(info.DefaultFullscreen);
	DontAskAgainCheckbox->SetChecked(!info.DefaultQueryIWAD);

	DisableAutoloadCheckbox->SetChecked(info.DefaultStartFlags & 1);
	LightsCheckbox->SetChecked(info.DefaultStartFlags & 2);
	BrightmapsCheckbox->SetChecked(info.DefaultStartFlags & 4);
	WidescreenCheckbox->SetChecked(info.DefaultStartFlags & 8);
	SupportWadsCheckbox->SetChecked(info.DefaultStartFlags & 16);

#ifdef RENDER_BACKENDS
	BackendLabel = new TextLabel(this);
	VulkanCheckbox = new CheckboxLabel(this);
	OpenGLCheckbox = new CheckboxLabel(this);
	GLESCheckbox = new CheckboxLabel(this);

	OpenGLCheckbox->SetRadioStyle(true);
	VulkanCheckbox->SetRadioStyle(true);
	GLESCheckbox->SetRadioStyle(true);
	OpenGLCheckbox->FuncChanged = [this](bool on) { if (on) { VulkanCheckbox->SetChecked(false); GLESCheckbox->SetChecked(false); }};
	VulkanCheckbox->FuncChanged = [this](bool on) { if (on) { OpenGLCheckbox->SetChecked(false); GLESCheckbox->SetChecked(false); }};
	GLESCheckbox->FuncChanged = [this](bool on) { if (on) { VulkanCheckbox->SetChecked(false); OpenGLCheckbox->SetChecked(false); }};
	switch (info.DefaultBackend)
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
	catch (const std::exception&)
	{
		hideLanguage = true;
	}
	int i = 0;
	for (auto& l : languages)
	{
		LangList->AddItem(l.second.GetChars());
		if (!l.first.CompareNoCase(info.DefaultLanguage))
			LangList->SetSelectedItem(i);
		++i;
	}
	LangList->OnChanged = [=](int i) { OnLanguageChanged(i); };

	{
		LoadLabel = new TextLabel(this);
		LoadList = new Dropdown(this);
		LoadList->SetMaxDisplayItems(2);
		LoadList->SetDropdownDirection(false);
		int opts = sizeof(FILELOAD_OPTS)/sizeof(FILELOAD_OPTS[0]), selected = opts-1;
		for (int i = 0; i < opts; i++)
		{
			LoadList->AddItem(GStrings.GetString(FILELOAD_OPTS[i].string));
			if (info.DefaultFileLoadBehaviour == FILELOAD_OPTS[i].flag) selected = i;
		}
		LoadList->SetSelectedItem(selected);
	}
}

void SettingsPage::SetValues(FStartupSelectionInfo& info) const
{
	info.DefaultFullscreen = FullscreenCheckbox->GetChecked();
	info.DefaultQueryIWAD = !DontAskAgainCheckbox->GetChecked();
	info.DefaultLanguage = languages[LangList->GetSelectedItem()].first.GetChars();

	int flags = 0;
	if (DisableAutoloadCheckbox->GetChecked()) flags |= 1;
	if (LightsCheckbox->GetChecked()) flags |= 2;
	if (BrightmapsCheckbox->GetChecked()) flags |= 4;
	if (WidescreenCheckbox->GetChecked()) flags |= 8;
	if (SupportWadsCheckbox->GetChecked()) flags |= 16;
	info.DefaultStartFlags = flags;

	int flBehaviour = FILELOAD_OPTS[LoadList->GetSelectedItem()].flag;
	if (flBehaviour != -1) info.DefaultFileLoadBehaviour = flBehaviour;

#ifdef RENDER_BACKENDS
	int v = 1;
	if (OpenGLCheckbox->GetChecked()) v = 0;
	else if (VulkanCheckbox->GetChecked()) v = 1;
	else if (GLESCheckbox->GetChecked()) v = 2;
	info.DefaultBackend = v;
#endif
}

void SettingsPage::UpdateLanguage()
{
	LangLabel->SetText(GStrings.GetString("OPTMNU_LANGUAGE"));
	LoadLabel->SetText(GStrings.GetString("PICKER_FILELOADING"));
	GeneralLabel->SetText(GStrings.GetString("PICKER_GENERAL"));
	ExtrasLabel->SetText(GStrings.GetString("PICKER_EXTRA"));
	FullscreenCheckbox->SetText(GStrings.GetString("PICKER_FULLSCREEN"));
	DisableAutoloadCheckbox->SetText(GStrings.GetString("PICKER_NOAUTOLOAD"));
	DontAskAgainCheckbox->SetText(GStrings.GetString("PICKER_DONTASK"));
	LightsCheckbox->SetText(GStrings.GetString("PICKER_LIGHTS"));
	BrightmapsCheckbox->SetText(GStrings.GetString("PICKER_BRIGHTMAPS"));
	WidescreenCheckbox->SetText(GStrings.GetString("PICKER_WIDESCREEN"));
	SupportWadsCheckbox->SetText(GStrings.GetString("PICKER_SUPPORTWADS"));

#ifdef RENDER_BACKENDS
	BackendLabel->SetText(GStrings.GetString("PICKER_PREFERBACKEND"));
	VulkanCheckbox->SetText(GStrings.GetString("OPTVAL_VULKAN"));
	OpenGLCheckbox->SetText(GStrings.GetString("OPTVAL_OPENGL"));
	GLESCheckbox->SetText(GStrings.GetString("OPTVAL_OPENGLES"));
#endif
}

void SettingsPage::OnLanguageChanged(int i)
{
	GStrings.UpdateLanguage(languages[i].first.GetChars());
	UpdateLanguage();
	Update();
	Launcher->UpdateLanguage();
}

void SettingsPage::OnGeometryChanged()
{
	double panelWidth = 160.0;
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

	SupportWadsCheckbox->SetFrameGeometry(0.0, y, 190.0, SupportWadsCheckbox->GetPreferredHeight());
	y += SupportWadsCheckbox->GetPreferredHeight() + 10.0;
	const double optionsBottom = y;

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

	y = max<double>(y, optionsBottom);
	if (!hideLanguage)
	{
		LangLabel->SetFrameGeometry(0.0, y, w, LangLabel->GetPreferredHeight());
		y += LangLabel->GetPreferredHeight();
		double temp = std::max(h - y - LoadList->GetPreferredHeight() - 4.0, 0.0);
		LangList->SetFrameGeometry(0.0, y, w, temp);
		y += temp + 4.0;
	}

	LoadLabel->SetFrameGeometry(
		0.0, y+(LoadList->GetPreferredHeight()-LoadLabel->GetPreferredHeight())/2,
		LoadLabel->GetPreferredWidth(), LoadLabel->GetPreferredHeight()
	);
	LoadList->SetFrameGeometry(
		LoadLabel->GetPreferredWidth()+4.0, y,
		LoadList->GetPreferredWidth(), LoadList->GetPreferredHeight()
	);
	y += LoadList->GetHeight();

	Launcher->UpdatePlayButton();
}
