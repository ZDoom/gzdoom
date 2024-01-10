#pragma once

#include <zwidget/core/widget.h>

class LauncherWindow;
class PushButton;

class LauncherButtonbar : public Widget
{
public:
	LauncherButtonbar(LauncherWindow* parent);
	void UpdateLanguage();

	double GetPreferredHeight() const;

private:
	void OnGeometryChanged() override;
	void OnPlayButtonClicked();
	void OnExitButtonClicked();

	LauncherWindow* GetLauncher() const;

	PushButton* PlayButton = nullptr;
	PushButton* ExitButton = nullptr;
};
