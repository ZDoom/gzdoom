#pragma once

#include <zwidget/core/widget.h>

#define RENDER_BACKENDS

class LauncherWindow;
class TextEdit;
class PushButton;
struct FStartupSelectionInfo;

class AboutPage : public Widget
{
public:
	AboutPage(LauncherWindow* launcher, const FStartupSelectionInfo& info);
	void UpdateLanguage();
	void SetValues(FStartupSelectionInfo& info) const;

private:
	void OnLanguageChanged(int i);
	void OnGeometryChanged() override;

	LauncherWindow* Launcher = nullptr;

	TextEdit* Text = nullptr;
	PushButton* Notes = nullptr;
};
