#pragma once

#include <zwidget/core/widget.h>
#include "gstrings.h"

class LauncherWindow;
class CheckboxLabel;
class LineEdit;
class ListView;
class TextLabel;
class PushButton;
class TabWidget;
struct WadStuff;
struct FStartupSelectionInfo;

class HostSubPage;
class JoinSubPage;

class NetworkPage : public Widget
{
public:
	NetworkPage(LauncherWindow* launcher, const FStartupSelectionInfo& info);
	void UpdateLanguage();
	void UpdatePlayButton();
	bool IsInHost() const;
	void SetValues(FStartupSelectionInfo& info) const;
	void InitializeTabs(const FStartupSelectionInfo& info);

private:
	void OnGeometryChanged() override;
	void OnSetFocus() override;
	void OnIWADsListActivated();

	LauncherWindow* Launcher = nullptr;

	// Direct hook into the play page for these so their editing is synchronized.
	LineEdit* ParametersEdit = nullptr;
	ListView* IWADsList = nullptr;
	TextLabel* ParametersLabel = nullptr;

	HostSubPage* HostPage = nullptr;
	JoinSubPage* JoinPage = nullptr;
	TabWidget* StartPages = nullptr;
	
	LineEdit* SaveFileEdit = nullptr;
	TextLabel* SaveFileLabel = nullptr;
	CheckboxLabel* SaveFileCheckbox = nullptr;
	CheckboxLabel* SaveArgsCheckbox = nullptr;
};

class HostSubPage : public Widget
{
public:
	HostSubPage(NetworkPage* main, const FStartupSelectionInfo& info);
	void UpdateLanguage();
	void SetValues(FStartupSelectionInfo& info) const;

private:
	void OnGeometryChanged() override;

	NetworkPage* MainTab = nullptr;

	TextLabel* NetModesLabel = nullptr;
	CheckboxLabel* AutoNetmodeCheckbox = nullptr;
	CheckboxLabel* PeerToPeerCheckbox = nullptr;
	CheckboxLabel* PacketServerCheckbox = nullptr;
	ListView* TicDupList = nullptr;
	TextLabel* TicDupLabel = nullptr;
	CheckboxLabel* ExtraTicCheckbox = nullptr;

	TextLabel* GameModesLabel = nullptr;
	CheckboxLabel* CoopCheckbox = nullptr;
	CheckboxLabel* DeathmatchCheckbox = nullptr;
	CheckboxLabel* AltDeathmatchCheckbox = nullptr;
	CheckboxLabel* TeamDeathmatchCheckbox = nullptr;
	TextLabel* TeamLabel = nullptr;
	LineEdit* TeamEdit = nullptr;

	LineEdit* MaxPlayersEdit = nullptr;
	LineEdit* PortEdit = nullptr;
	TextLabel* MaxPlayersLabel = nullptr;
	TextLabel* PortLabel = nullptr;

	TextLabel* MaxPlayerHintLabel = nullptr;
	TextLabel* PortHintLabel = nullptr;
	TextLabel* TeamHintLabel = nullptr;
};

class JoinSubPage : public Widget
{
public:
	JoinSubPage(NetworkPage* main, const FStartupSelectionInfo& info);
	void UpdateLanguage();
	void SetValues(FStartupSelectionInfo& info) const;

private:
	void OnGeometryChanged() override;

	NetworkPage* MainTab = nullptr;

	LineEdit* AddressEdit = nullptr;
	LineEdit* AddressPortEdit = nullptr;
	TextLabel* AddressLabel = nullptr;
	TextLabel* AddressPortLabel = nullptr;

	TextLabel* TeamDeathmatchLabel = nullptr;
	TextLabel* TeamLabel = nullptr;
	LineEdit* TeamEdit = nullptr;

	TextLabel* AddressPortHintLabel = nullptr;
	TextLabel* TeamHintLabel = nullptr;
};
