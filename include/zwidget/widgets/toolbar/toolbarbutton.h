
#pragma once

#include "../../core/widget.h"
#include "../textlabel/textlabel.h"
#include "../imagebox/imagebox.h"

class ToolbarButton : public Widget
{
public:
	ToolbarButton(Widget* parent);
	~ToolbarButton();

	void SetIcon(std::string icon);
	void SetText(std::string text);

	void Click();

	double GetPreferredWidth();
	double GetPreferredHeight();

	std::function<void()> OnClick;

protected:
	void OnMouseMove(const Point& pos) override;
	void OnMouseLeave() override;
	bool OnMouseDown(const Point& pos, InputKey key) override;
	bool OnMouseUp(const Point& pos, InputKey key) override;
	void OnGeometryChanged() override;

private:
	ImageBox* image = nullptr;
	TextLabel* label = nullptr;
};
