#pragma once

#include <zwidget/core/widget.h>


#define RENDER_BACKENDS

class ImageBox;
class TextLabel;
class CheckboxLabel;
class PushButton;
class ListView;
class LineEdit;
struct WadStuff;

class LauncherWindow : public Widget
{
public:
	static int ExecModal(WadStuff* wads, int numwads, int defaultiwad, int* autoloadflags);

	LauncherWindow(WadStuff* wads, int numwads, int defaultiwad, int* autoloadflags);

private:
	void OnPlayButtonClicked();
	void OnExitButtonClicked();
	void OnGamesListActivated();

	void OnClose() override;
	void OnGeometryChanged() override;

	ImageBox* Logo = nullptr;
	TextLabel* WelcomeLabel = nullptr;
	TextLabel* VersionLabel = nullptr;
	TextLabel* SelectLabel = nullptr;
	TextLabel* GeneralLabel = nullptr;
	TextLabel* ExtrasLabel = nullptr;
	TextLabel* ParametersLabel = nullptr;
	CheckboxLabel* FullscreenCheckbox = nullptr;
	CheckboxLabel* DisableAutoloadCheckbox = nullptr;
	CheckboxLabel* DontAskAgainCheckbox = nullptr;
	CheckboxLabel* LightsCheckbox = nullptr;
	CheckboxLabel* BrightmapsCheckbox = nullptr;
	CheckboxLabel* WidescreenCheckbox = nullptr;
#ifdef RENDER_BACKENDS
	TextLabel* BackendLabel = nullptr;
	CheckboxLabel* VulkanCheckbox = nullptr;
	CheckboxLabel* OpenGLCheckbox = nullptr;
	CheckboxLabel* GLESCheckbox = nullptr;
#endif
	PushButton* PlayButton = nullptr;
	PushButton* ExitButton = nullptr;
	ListView* GamesList = nullptr;
	LineEdit* ParametersEdit = nullptr;

	int* AutoloadFlags = nullptr;
	int ExecResult = -1;
};
