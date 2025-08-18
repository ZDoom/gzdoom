#pragma once

#include <zwidget/core/widget.h>
#include "gstrings.h"

class LauncherWindow;
class CheckboxLabel;
class LineEdit;
class Dropdown;
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

	Dropdown* IWADsDropdown = nullptr;
	TextLabel* SaveFileLabel = nullptr;
	LineEdit* SaveFileEdit = nullptr;
	CheckboxLabel* SaveFileCheckbox = nullptr;
	
	HostSubPage* HostPage = nullptr;
	JoinSubPage* JoinPage = nullptr;
	TabWidget* StartPages = nullptr;
	
	TextLabel* ParametersLabel = nullptr;
	LineEdit* ParametersEdit = nullptr;
	CheckboxLabel* SaveParametersCheckbox = nullptr;
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
	Dropdown* NetModesDropdown = nullptr;
	TextLabel* TicDupLabel = nullptr;
	Dropdown* TicDupDropdown = nullptr;
	CheckboxLabel* ExtraTicCheckbox = nullptr;

	TextLabel* GameModesLabel = nullptr;
	Dropdown* GameModesDropdown = nullptr;
	CheckboxLabel* AltDeathmatchCheckbox = nullptr;
	TextLabel* TeamLabel = nullptr;
	LineEdit* TeamEdit = nullptr;
	TextLabel* TeamHintLabel = nullptr;

	TextLabel* MaxPlayersLabel = nullptr;
	LineEdit* MaxPlayersEdit = nullptr;
	TextLabel* MaxPlayerHintLabel = nullptr;
	TextLabel* PortLabel = nullptr;
	LineEdit* PortEdit = nullptr;
	TextLabel* PortHintLabel = nullptr;
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

	TextLabel* AddressLabel = nullptr;
	LineEdit* AddressEdit = nullptr;
	TextLabel* AddressPortLabel = nullptr;
	LineEdit* AddressPortEdit = nullptr;
	TextLabel* AddressPortHintLabel = nullptr;
	
	TextLabel* TeamDeathmatchLabel = nullptr;
	TextLabel* TeamLabel = nullptr;
	LineEdit* TeamEdit = nullptr;
	TextLabel* TeamHintLabel = nullptr;
};
