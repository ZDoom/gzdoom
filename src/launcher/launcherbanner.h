#pragma once

#include <zwidget/core/widget.h>

class ImageBox;
class TextLabel;

class LauncherBanner : public Widget
{
public:
	LauncherBanner(Widget* parent);
	void UpdateLanguage();

	double GetPreferredHeight() const;

private:
	void OnGeometryChanged() override;

	ImageBox* Logo = nullptr;
	TextLabel* VersionLabel = nullptr;
};
