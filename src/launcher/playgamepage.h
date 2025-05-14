#pragma once

#include <zwidget/core/widget.h>

class LauncherWindow;
class TextLabel;
class ListView;
class LineEdit;
struct WadStuff;

class PlayGamePage : public Widget
{
public:
	PlayGamePage(LauncherWindow* launcher, WadStuff* wads, int numwads, int defaultiwad);
	void UpdateLanguage();

	void SetExtraArgs(const std::string& args);
	std::string GetExtraArgs();
	int GetSelectedGame();

private:
	void OnGeometryChanged() override;
	void OnSetFocus() override;
	void OnGamesListActivated();

	LauncherWindow* Launcher = nullptr;

	TextLabel* WelcomeLabel = nullptr;
	TextLabel* SelectLabel = nullptr;
	TextLabel* ParametersLabel = nullptr;
	ListView* GamesList = nullptr;
	LineEdit* ParametersEdit = nullptr;
};
