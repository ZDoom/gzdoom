
#include "networkpage.h"
#include "launcherwindow.h"
#include "gstrings.h"
#include "c_cvars.h"
#include "i_net.h"
#include "i_interface.h"
#include <zwidget/core/resourcedata.h>
#include <zwidget/widgets/listview/listview.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <zwidget/widgets/checkboxlabel/checkboxlabel.h>
#include <zwidget/widgets/lineedit/lineedit.h>
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/tabwidget/tabwidget.h>

constexpr double EditHeight = 24.0;

NetworkPage::NetworkPage(LauncherWindow* launcher, const FStartupSelectionInfo& info) : Widget(nullptr), Launcher(launcher)
{
	ParametersEdit = new LineEdit(this);
	ParametersLabel = new TextLabel(this);
	SaveFileEdit = new LineEdit(this);
	SaveFileLabel = new TextLabel(this);
	SaveFileCheckbox = new CheckboxLabel(this);
	SaveArgsCheckbox = new CheckboxLabel(this);
	IWADsList = new ListView(this);

	SaveFileCheckbox->SetChecked(info.bSaveNetFile);
	if (!info.DefaultNetSaveFile.IsEmpty())
		SaveFileEdit->SetText(info.DefaultNetSaveFile.GetChars());

	SaveArgsCheckbox->SetChecked(info.bSaveNetArgs);
	if (!info.DefaultNetArgs.IsEmpty())
		ParametersEdit->SetText(info.DefaultNetArgs.GetChars());

	StartPages = new TabWidget(this);
	HostPage = new HostSubPage(this, info);
	JoinPage = new JoinSubPage(this, info);

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

		IWADsList->AddItem(work.GetChars());
	}

	if (info.DefaultNetIWAD >= 0 && info.DefaultNetIWAD < info.Wads->SSize())
	{
		IWADsList->SetSelectedItem(info.DefaultNetIWAD);
		IWADsList->ScrollToItem(info.DefaultNetIWAD);
	}

	IWADsList->OnActivated = [=]() { OnIWADsListActivated(); };
}

// This has to be done after the main page is parented, otherwise it won't have the correct
// info to pull from.
void NetworkPage::InitializeTabs(const FStartupSelectionInfo& info)
{
	StartPages->AddTab(HostPage, "Host");
	StartPages->AddTab(JoinPage, "Join");

	switch (info.DefaultNetPage)
	{
	case 1:
		StartPages->SetCurrentWidget(JoinPage);
		break;
	default:
		StartPages->SetCurrentWidget(HostPage);
		break;
	}
}

void NetworkPage::OnIWADsListActivated()
{
	Launcher->Start();
}

void NetworkPage::OnSetFocus()
{
	IWADsList->SetFocus();
}

void NetworkPage::SetValues(FStartupSelectionInfo& info) const
{
	info.DefaultNetIWAD = IWADsList->GetSelectedItem();
	info.DefaultNetArgs = ParametersEdit->GetText();

	info.bHosting = IsInHost();
	if (info.bHosting)
	{
		info.DefaultNetPage = 0;
		HostPage->SetValues(info);
	}
	else
	{
		info.DefaultNetPage = 1;
		JoinPage->SetValues(info);
	}

	info.bSaveNetFile = SaveFileCheckbox->GetChecked();
	info.bSaveNetArgs = SaveArgsCheckbox->GetChecked();
	const auto save = SaveFileEdit->GetText();
	if (!save.empty())
		info.AdditionalNetArgs.AppendFormat(" -loadgame %s", save.c_str());
	info.DefaultNetSaveFile = save;
}

void NetworkPage::UpdatePlayButton()
{
	Launcher->UpdatePlayButton();
}

bool NetworkPage::IsInHost() const
{
	return StartPages->GetCurrentIndex() >= 0 ? StartPages->GetCurrentWidget() == HostPage : false;
}

void NetworkPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	const double wSize = w * 0.45 - 2.5;

	double y = h - SaveArgsCheckbox->GetPreferredHeight();
	SaveArgsCheckbox->SetFrameGeometry(0.0, y, wSize, SaveArgsCheckbox->GetPreferredHeight());

	y -= SaveFileCheckbox->GetPreferredHeight();
	SaveFileCheckbox->SetFrameGeometry(0.0, y, wSize, SaveFileCheckbox->GetPreferredHeight());

	y -= EditHeight + 2.0;
	ParametersEdit->SetFrameGeometry(0.0, y, wSize, EditHeight);
	y -= ParametersLabel->GetPreferredHeight();
	ParametersLabel->SetFrameGeometry(0.0, y, wSize, ParametersLabel->GetPreferredHeight());

	y -= EditHeight + 2.0;
	SaveFileEdit->SetFrameGeometry(0.0, y, wSize, EditHeight);
	y -= SaveFileLabel->GetPreferredHeight();
	SaveFileLabel->SetFrameGeometry(0.0, y, wSize, SaveFileLabel->GetPreferredHeight());
	y -= 5.0;

	IWADsList->SetFrameGeometry(0.0, 0.0, wSize, y);

	const double xOfs = w * 0.45 + 2.5;
	StartPages->SetFrameGeometry(xOfs, 0.0, w - xOfs, h);
}

void NetworkPage::UpdateLanguage()
{
	ParametersLabel->SetText(GStrings.GetString("PICKER_ADDPARM"));
	SaveFileLabel->SetText(GStrings.GetString("PICKER_LOADSAVE"));
	SaveFileCheckbox->SetText(GStrings.GetString("PICKER_REMSAVE"));
	SaveArgsCheckbox->SetText(GStrings.GetString("PICKER_REMPARM"));

	StartPages->SetTabText(HostPage, GStrings.GetString("PICKER_HOST"));
	StartPages->SetTabText(JoinPage, GStrings.GetString("PICKER_JOIN"));
	HostPage->UpdateLanguage();
	JoinPage->UpdateLanguage();
}

HostSubPage::HostSubPage(NetworkPage* main, const FStartupSelectionInfo& info) : Widget(nullptr), MainTab(main)
{
	NetModesLabel = new TextLabel(this);
	AutoNetmodeCheckbox = new CheckboxLabel(this);
	PacketServerCheckbox = new CheckboxLabel(this);
	PeerToPeerCheckbox = new CheckboxLabel(this);

	AutoNetmodeCheckbox->SetRadioStyle(true);
	PacketServerCheckbox->SetRadioStyle(true);
	PeerToPeerCheckbox->SetRadioStyle(true);
	AutoNetmodeCheckbox->FuncChanged = [this](bool on) { if (on) { PacketServerCheckbox->SetChecked(false); PeerToPeerCheckbox->SetChecked(false); }};
	PacketServerCheckbox->FuncChanged = [this](bool on) { if (on) { AutoNetmodeCheckbox->SetChecked(false); PeerToPeerCheckbox->SetChecked(false); }};
	PeerToPeerCheckbox->FuncChanged = [this](bool on) { if (on) { PacketServerCheckbox->SetChecked(false); AutoNetmodeCheckbox->SetChecked(false); }};
	
	switch (info.DefaultNetMode)
	{
	case 0:
		PeerToPeerCheckbox->SetChecked(true);
		break;
	case 1:
		PacketServerCheckbox->SetChecked(true);
		break;
	default:
		AutoNetmodeCheckbox->SetChecked(true);
		break;
	}

	TicDupList = new ListView(this);
	TicDupLabel = new TextLabel(this);
	ExtraTicCheckbox = new CheckboxLabel(this);

	std::vector<double> widths = { 30.0, 30.0 };
	TicDupList->SetColumnWidths(widths);
	TicDupList->AddItem("35.0");
	TicDupList->UpdateItem("Hz", 0, 1);
	TicDupList->AddItem("17.5");
	TicDupList->UpdateItem("Hz", 1, 1);
	TicDupList->AddItem("11.6");
	TicDupList->UpdateItem("Hz", 2, 1);
	TicDupList->SetSelectedItem(info.DefaultNetTicDup);
	ExtraTicCheckbox->SetChecked(info.DefaultNetExtraTic);

	GameModesLabel = new TextLabel(this);
	CoopCheckbox = new CheckboxLabel(this);
	DeathmatchCheckbox = new CheckboxLabel(this);
	AltDeathmatchCheckbox = new CheckboxLabel(this);
	TeamDeathmatchCheckbox = new CheckboxLabel(this);
	TeamLabel = new TextLabel(this);
	TeamEdit = new LineEdit(this);

	// These are intentionally not radio buttons, they just act similarly for clarity in the UI.
	CoopCheckbox->FuncChanged = [this](bool on) { if (on) { DeathmatchCheckbox->SetChecked(false); TeamDeathmatchCheckbox->SetChecked(false); }};
	DeathmatchCheckbox->FuncChanged = [this](bool on) { if (on) { CoopCheckbox->SetChecked(false); TeamDeathmatchCheckbox->SetChecked(false); }};
	TeamDeathmatchCheckbox->FuncChanged = [this](bool on) { if (on) { CoopCheckbox->SetChecked(false); DeathmatchCheckbox->SetChecked(false); }};

	switch (info.DefaultNetGameMode)
	{
	case 0:
		CoopCheckbox->SetChecked(true);
		break;
	case 1:
		DeathmatchCheckbox->SetChecked(true);
		break;
	case 2:
		TeamDeathmatchCheckbox->SetChecked(true);
		break;
	}
	AltDeathmatchCheckbox->SetChecked(info.DefaultNetAltDM);

	TeamEdit->SetMaxLength(3);
	TeamEdit->SetNumericMode(true);
	TeamEdit->SetTextInt(info.DefaultNetHostTeam);

	MaxPlayersEdit = new LineEdit(this);
	PortEdit = new LineEdit(this);
	MaxPlayersLabel = new TextLabel(this);
	PortLabel = new TextLabel(this);

	MaxPlayersEdit->SetMaxLength(2);
	MaxPlayersEdit->SetNumericMode(true);
	MaxPlayersEdit->SetTextInt(info.DefaultNetPlayers);
	PortEdit->SetMaxLength(5);
	PortEdit->SetNumericMode(true);
	if (info.DefaultNetHostPort > 0)
		PortEdit->SetTextInt(info.DefaultNetHostPort);

	MaxPlayerHintLabel = new TextLabel(this);
	PortHintLabel = new TextLabel(this);
	TeamHintLabel = new TextLabel(this);

	MaxPlayerHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
	PortHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
	TeamHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
}

void HostSubPage::SetValues(FStartupSelectionInfo& info) const
{
	info.AdditionalNetArgs = "";
	if (PacketServerCheckbox->GetChecked())
	{
		info.AdditionalNetArgs.AppendFormat(" -netmode 1");
		info.DefaultNetMode = 1;
	}
	else if (PeerToPeerCheckbox->GetChecked())
	{
		info.AdditionalNetArgs.AppendFormat(" -netmode 0");
		info.DefaultNetMode = 0;
	}
	else
	{
		info.DefaultNetMode = -1;
	}

	info.DefaultNetExtraTic = ExtraTicCheckbox->GetChecked();
	if (info.DefaultNetExtraTic)
		info.AdditionalNetArgs.AppendFormat(" -extratic");

	const int dup = TicDupList->GetSelectedItem();
	if (dup > 0)
		info.AdditionalNetArgs.AppendFormat(" -dup %d", dup + 1);
	info.DefaultNetTicDup = dup;

	info.DefaultNetPlayers = clamp<int>(MaxPlayersEdit->GetTextInt(), 1, MAXPLAYERS);
	info.AdditionalNetArgs.AppendFormat(" -host %d", info.DefaultNetPlayers);
	const int port = clamp<int>(PortEdit->GetTextInt(), 0, UINT16_MAX);
	if (port > 0)
	{
		info.AdditionalNetArgs.AppendFormat(" -port %d", port);
		info.DefaultNetHostPort = port;
	}
	else
	{
		info.DefaultNetHostPort = 0;
	}

	info.DefaultNetAltDM = AltDeathmatchCheckbox->GetChecked();
	if (CoopCheckbox->GetChecked())
	{
		info.AdditionalNetArgs.AppendFormat(" -coop");
		info.DefaultNetGameMode = 0;
	}
	else if (DeathmatchCheckbox->GetChecked())
	{
		if (AltDeathmatchCheckbox->GetChecked())
			info.AdditionalNetArgs.AppendFormat(" -altdeath");
		else
			info.AdditionalNetArgs.AppendFormat(" -deathmatch");

		info.DefaultNetGameMode = 1;
	}
	else if (TeamDeathmatchCheckbox->GetChecked())
	{
		if (AltDeathmatchCheckbox->GetChecked())
			info.AdditionalNetArgs.AppendFormat(" -altdeath");
		else
			info.AdditionalNetArgs.AppendFormat(" -deathmatch");
		info.AdditionalNetArgs.AppendFormat(" +teamplay 1");
		info.DefaultNetGameMode = 2;

		int team = 255;
		if (!TeamEdit->GetText().empty())
		{
			team = TeamEdit->GetTextInt();
			if (team < 0 || team > 255)
				team = 255;
		}
		info.AdditionalNetArgs.AppendFormat(" +team %d", team);
		info.DefaultNetHostTeam = team;
	}
	else
	{
		info.DefaultNetGameMode = -1;
	}
}

void HostSubPage::UpdateLanguage()
{
	NetModesLabel->SetText(GStrings.GetString("PICKER_NETMODE"));
	AutoNetmodeCheckbox->SetText(GStrings.GetString("PICKER_NETAUTO"));
	PacketServerCheckbox->SetText(GStrings.GetString("PICKER_NETSERVER"));
	PeerToPeerCheckbox->SetText(GStrings.GetString("PICKER_NETPEER"));

	TicDupLabel->SetText(GStrings.GetString("PICKER_NETRATE"));
	ExtraTicCheckbox->SetText(GStrings.GetString("PICKER_NETBACKUP"));

	GameModesLabel->SetText(GStrings.GetString("PICKER_GAMEMODE"));
	CoopCheckbox->SetText(GStrings.GetString("PICKER_COOP"));
	DeathmatchCheckbox->SetText(GStrings.GetString("PICKER_DM"));
	AltDeathmatchCheckbox->SetText(GStrings.GetString("PICKER_ALTDM"));
	TeamDeathmatchCheckbox->SetText(GStrings.GetString("PICKER_TDM"));
	TeamLabel->SetText(GStrings.GetString("PICKER_TEAM"));

	MaxPlayersLabel->SetText(GStrings.GetString("PICKER_PLAYERS"));
	PortLabel->SetText(GStrings.GetString("PICKER_PORT"));

	MaxPlayerHintLabel->SetText(GStrings.GetString("PICKER_PLAYERHINT"));
	PortHintLabel->SetText(GStrings.GetString("PICKER_PORTHINT"));
	TeamHintLabel->SetText(GStrings.GetString("PICKER_TEAMHINT"));
}

void HostSubPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	constexpr double LabelOfsSize = 90.0;
	constexpr double hintOfs = 170.0;

	double y = 0.0;

	MaxPlayersLabel->SetFrameGeometry(0.0, y, LabelOfsSize, MaxPlayersLabel->GetPreferredHeight());
	MaxPlayersEdit->SetFrameGeometry(MaxPlayersLabel->GetWidth(), y, 30.0, EditHeight);
	y += EditHeight + 2.0;

	PortLabel->SetFrameGeometry(0.0, y, LabelOfsSize, PortLabel->GetPreferredHeight());
	PortEdit->SetFrameGeometry(PortLabel->GetWidth(), y, 60.0, EditHeight);

	MaxPlayerHintLabel->SetFrameGeometry(hintOfs, 0.0, w - hintOfs, MaxPlayerHintLabel->GetPreferredHeight());
	PortHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, PortHintLabel->GetPreferredHeight());

	y += EditHeight + 7.0;

	const double optionsTop = y;
	TicDupLabel->SetFrameGeometry(0.0, y, 100.0, TicDupLabel->GetPreferredHeight());
	y += TicDupLabel->GetPreferredHeight();
	TicDupList->SetFrameGeometry(0.0, y, 100.0, (TicDupList->GetItemAmount() + 1) * 20.0);
	y += TicDupList->GetHeight() + ExtraTicCheckbox->GetPreferredHeight() + 2.0;

	ExtraTicCheckbox->SetFrameGeometry(0.0, y, w, ExtraTicCheckbox->GetPreferredHeight());
	y += ExtraTicCheckbox->GetPreferredHeight();

	const double optionsBottom = y;
	y = optionsTop;

	constexpr double NetModeXOfs = 115.0;
	NetModesLabel->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, NetModesLabel->GetPreferredHeight());
	y += NetModesLabel->GetPreferredHeight();

	AutoNetmodeCheckbox->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, AutoNetmodeCheckbox->GetPreferredHeight());
	y += AutoNetmodeCheckbox->GetPreferredHeight();

	PacketServerCheckbox->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, PacketServerCheckbox->GetPreferredHeight());
	y += PacketServerCheckbox->GetPreferredHeight();

	PeerToPeerCheckbox->SetFrameGeometry(NetModeXOfs, y, w - NetModeXOfs, PeerToPeerCheckbox->GetPreferredHeight());
	y += PeerToPeerCheckbox->GetPreferredHeight();

	y = max<int>(optionsBottom, y) + 10.0;
	GameModesLabel->SetFrameGeometry(0.0, y, w, GameModesLabel->GetPreferredHeight());
	y += GameModesLabel->GetPreferredHeight();

	CoopCheckbox->SetFrameGeometry(0.0, y, w, CoopCheckbox->GetPreferredHeight());
	y += CoopCheckbox->GetPreferredHeight();

	DeathmatchCheckbox->SetFrameGeometry(0.0, y, w, DeathmatchCheckbox->GetPreferredHeight());
	y += DeathmatchCheckbox->GetPreferredHeight();

	TeamDeathmatchCheckbox->SetFrameGeometry(0.0, y, w, TeamDeathmatchCheckbox->GetPreferredHeight());
	y += TeamDeathmatchCheckbox->GetPreferredHeight() + 2.0;

	TeamLabel->SetFrameGeometry(14.0, y, LabelOfsSize - 14.0, TeamLabel->GetPreferredHeight());
	TeamEdit->SetFrameGeometry(TeamLabel->GetWidth() + 14.0, y, 45.0, EditHeight);
	TeamHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, TeamHintLabel->GetPreferredHeight());
	y += EditHeight + 2.0;

	AltDeathmatchCheckbox->SetFrameGeometry(0.0, y, w, AltDeathmatchCheckbox->GetPreferredHeight());

	MainTab->UpdatePlayButton();
}

JoinSubPage::JoinSubPage(NetworkPage* main, const FStartupSelectionInfo& info) : Widget(nullptr), MainTab(main)
{
	AddressEdit = new LineEdit(this);
	AddressPortEdit = new LineEdit(this);
	AddressLabel = new TextLabel(this);
	AddressPortLabel = new TextLabel(this);

	AddressEdit->SetText(info.DefaultNetAddress.GetChars());
	AddressPortEdit->SetMaxLength(5);
	AddressPortEdit->SetNumericMode(true);
	if (info.DefaultNetJoinPort > 0)
		AddressPortEdit->SetTextInt(info.DefaultNetJoinPort);

	TeamDeathmatchLabel = new TextLabel(this);
	TeamLabel = new TextLabel(this);
	TeamEdit = new LineEdit(this);

	TeamEdit->SetMaxLength(3);
	TeamEdit->SetNumericMode(true);
	TeamEdit->SetTextInt(info.DefaultNetJoinTeam);

	AddressPortHintLabel = new TextLabel(this);
	TeamHintLabel = new TextLabel(this);

	AddressPortHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
	TeamHintLabel->SetStyleColor("color", Colorf::fromRgba8(160, 160, 160));
}

void JoinSubPage::SetValues(FStartupSelectionInfo& info) const
{
	FString addr = AddressEdit->GetText();
	info.DefaultNetAddress = addr;
	const int port = clamp<int>(AddressPortEdit->GetTextInt(), 0, UINT16_MAX);
	if (port > 0)
	{
		addr.AppendFormat(":%d", port);
		info.DefaultNetJoinPort = port;
	}
	else
	{
		info.DefaultNetJoinPort = 0;
	}

	info.AdditionalNetArgs = "";
	info.AdditionalNetArgs.AppendFormat(" -join %s", addr.GetChars());

	int team = 255;
	if (!TeamEdit->GetText().empty())
	{
		team = TeamEdit->GetTextInt();
		if (team < 0 || team > 255)
			team = 255;
	}
	info.AdditionalNetArgs.AppendFormat(" +team %d", team);
	info.DefaultNetJoinTeam = team;
}

void JoinSubPage::UpdateLanguage()
{
	AddressLabel->SetText(GStrings.GetString("PICKER_IP"));
	AddressPortLabel->SetText(GStrings.GetString("PICKER_PORT"));

	TeamDeathmatchLabel->SetText(GStrings.GetString("PICKER_TDMLABEL"));
	TeamLabel->SetText(GStrings.GetString("PICKER_TEAM"));

	AddressPortHintLabel->SetText(GStrings.GetString("PICKER_PORTHINT"));
	TeamHintLabel->SetText(GStrings.GetString("PICKER_TEAMHINT"));
}

void JoinSubPage::OnGeometryChanged()
{
	const double w = GetWidth();
	const double h = GetHeight();

	constexpr double LabelOfsSize = 90.0;
	constexpr double hintOfs = 170.0;

	double y = 0.0;

	AddressLabel->SetFrameGeometry(0.0, y, LabelOfsSize, AddressLabel->GetPreferredHeight());
	AddressEdit->SetFrameGeometry(AddressLabel->GetWidth(), y, 120.0, EditHeight);
	y += EditHeight + 2.0;

	AddressPortLabel->SetFrameGeometry(0.0, y, LabelOfsSize, AddressPortLabel->GetPreferredHeight());
	AddressPortEdit->SetFrameGeometry(AddressPortLabel->GetWidth(), y, 60.0, EditHeight);

	AddressPortHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, AddressPortHintLabel->GetPreferredHeight());
	y += EditHeight + 12.0;

	TeamDeathmatchLabel->SetFrameGeometry(0.0, y, w, TeamDeathmatchLabel->GetPreferredHeight());
	y += TeamDeathmatchLabel->GetPreferredHeight();

	TeamLabel->SetFrameGeometry(0.0, y, LabelOfsSize, TeamLabel->GetPreferredHeight());
	TeamEdit->SetFrameGeometry(TeamLabel->GetWidth(), y, 45.0, EditHeight);
	TeamHintLabel->SetFrameGeometry(hintOfs, y, w - hintOfs, TeamHintLabel->GetPreferredHeight());

	MainTab->UpdatePlayButton();
}
