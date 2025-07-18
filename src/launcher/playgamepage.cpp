
#include "playgamepage.h"
#include "launcherwindow.h"
#include "i_interface.h"
#include "gstrings.h"
#include "version.h"
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/lineedit/lineedit.h>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>

PlayGamePage::PlayGamePage(LauncherWindow* launcher, const FStartupSelectionInfo& info) : Widget(nullptr), Launcher(launcher)
{
	WelcomeLabel = new TextLabel(this);
	VersionLabel = new TextLabel(this);
	VersionLabel->SetTextAlignment(TextLabelAlignment::Right);
	SelectLabel = new TextLabel(this);
	ParametersLabel = new TextLabel(this);
	GamesList = new ListView(this);
	ParametersEdit = new LineEdit(this);
	SaveArgsCheckbox = new CheckboxLabel(this);

	SaveArgsCheckbox->SetChecked(info.bSaveArgs);
	if (!info.DefaultArgs.IsEmpty())
		ParametersEdit->SetText(info.DefaultArgs.GetChars());

	for (const auto& wad : *info.Wads)
	{
		const char* filepart = strrchr(wad.Path.GetChars(), '/');
		if (filepart == nullptr)
			filepart = wad.Path.GetChars();
		else
			++filepart;

		FString work;
		if (*filepart)
			work.Format("%s (%s)", wad.Name.GetChars(), filepart);
		else
			work = wad.Name.GetChars();

		GamesList->AddItem(work.GetChars());
	}

	if (info.DefaultIWAD >= 0 && info.DefaultIWAD < info.Wads->SSize())
	{
		GamesList->SetSelectedItem(info.DefaultIWAD);
		GamesList->ScrollToItem(info.DefaultIWAD);
	}

	GamesList->OnActivated = [=]() { OnGamesListActivated(); };
}

void PlayGamePage::SetValues(FStartupSelectionInfo& info) const
{
	info.DefaultIWAD = GamesList->GetSelectedItem();
	info.DefaultArgs = ParametersEdit->GetText();
	info.bSaveArgs = SaveArgsCheckbox->GetChecked();
}

void PlayGamePage::UpdateLanguage()
{
	SelectLabel->SetText(GStrings.GetString("PICKER_SELECT"));
	ParametersLabel->SetText(GStrings.GetString("PICKER_ADDPARM"));
	FString welcomeText = GStrings.GetString("PICKER_WELCOME");
	welcomeText.Substitute("%s", GAMENAME);
	WelcomeLabel->SetText(welcomeText.GetChars());
	FString versionText = GStrings.GetString("PICKER_VERSION");
	versionText.Substitute("%s", GetVersionString());
	VersionLabel->SetText(versionText.GetChars());
	SaveArgsCheckbox->SetText(GStrings.GetString("PICKER_REMPARM"));
}

void PlayGamePage::OnGamesListActivated()
{
	Launcher->Start();
}

void PlayGamePage::OnSetFocus()
{
	GamesList->SetFocus();
}

void PlayGamePage::OnGeometryChanged()
{
	double y = 10.0;

	WelcomeLabel->SetFrameGeometry(0.0, y, GetWidth(), WelcomeLabel->GetPreferredHeight());
	y += WelcomeLabel->GetPreferredHeight();

	VersionLabel->SetFrameGeometry(20.0, y - VersionLabel->GetPreferredHeight(), GetWidth() - 19.0, VersionLabel->GetPreferredHeight());

	y += 10.0;

	SelectLabel->SetFrameGeometry(0.0, y, GetWidth(), SelectLabel->GetPreferredHeight());
	y += SelectLabel->GetPreferredHeight();

	double listViewTop = y;

	y = GetHeight() - 10.0 - SaveArgsCheckbox->GetPreferredHeight();
	SaveArgsCheckbox->SetFrameGeometry(0.0, y, GetWidth(), SaveArgsCheckbox->GetPreferredHeight());
	y -= 2.0;

	double editHeight = 24.0;
	y -= editHeight;
	ParametersEdit->SetFrameGeometry(0.0, y, GetWidth(), editHeight);

	double labelHeight = ParametersLabel->GetPreferredHeight();
	y -= labelHeight;
	ParametersLabel->SetFrameGeometry(0.0, y, GetWidth(), labelHeight);
	y -= 10.0;

	double listViewBottom = y - 10.0;
	GamesList->SetFrameGeometry(0.0, listViewTop, GetWidth(), std::max(listViewBottom - listViewTop, 0.0));

	Launcher->UpdatePlayButton();
}
