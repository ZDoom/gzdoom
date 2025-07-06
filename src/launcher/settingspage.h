#pragma once

#include <zwidget/core/widget.h>
#include "gstrings.h"

#define RENDER_BACKENDS

class LauncherWindow;
class TextLabel;
class CheckboxLabel;
class ListView;
struct FStartupSelectionInfo;

class SettingsPage : public Widget
{
public:
	SettingsPage(LauncherWindow* launcher, const FStartupSelectionInfo& info);
	void UpdateLanguage();
	void SetValues(FStartupSelectionInfo& info) const;

private:
	void OnLanguageChanged(int i);
	void OnGeometryChanged() override;

	LauncherWindow* Launcher = nullptr;

	TextLabel* LangLabel = nullptr;
	TextLabel* GeneralLabel = nullptr;
	TextLabel* ExtrasLabel = nullptr;
	CheckboxLabel* FullscreenCheckbox = nullptr;
	CheckboxLabel* DisableAutoloadCheckbox = nullptr;
	CheckboxLabel* DontAskAgainCheckbox = nullptr;
	CheckboxLabel* LightsCheckbox = nullptr;
	CheckboxLabel* BrightmapsCheckbox = nullptr;
	CheckboxLabel* WidescreenCheckbox = nullptr;
	CheckboxLabel* SupportWadsCheckbox = nullptr;
#ifdef RENDER_BACKENDS
	TextLabel* BackendLabel = nullptr;
	CheckboxLabel* VulkanCheckbox = nullptr;
	CheckboxLabel* OpenGLCheckbox = nullptr;
	CheckboxLabel* GLESCheckbox = nullptr;
#endif
	ListView* LangList = nullptr;

	TArray<std::pair<FString, FString>> languages;
	bool hideLanguage = false;
};
