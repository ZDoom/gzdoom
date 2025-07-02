#pragma once

#include <zwidget/core/widget.h>

class LauncherWindow;
class TextLabel;
class ListView;
class LineEdit;
class CheckboxLabel;
struct WadStuff;
struct FStartupSelectionInfo;

class PlayGamePage : public Widget
{
public:
	PlayGamePage(LauncherWindow* launcher, const FStartupSelectionInfo& info);
	void UpdateLanguage();
	void SetValues(FStartupSelectionInfo& info) const;

private:
	void OnGeometryChanged() override;
	void OnSetFocus() override;
	void OnGamesListActivated();

	LauncherWindow* Launcher = nullptr;

	TextLabel* WelcomeLabel = nullptr;
	TextLabel* VersionLabel = nullptr;
	TextLabel* SelectLabel = nullptr;
	TextLabel* ParametersLabel = nullptr;
	ListView* GamesList = nullptr;
	LineEdit* ParametersEdit = nullptr;
	CheckboxLabel* SaveArgsCheckbox = nullptr;
};
