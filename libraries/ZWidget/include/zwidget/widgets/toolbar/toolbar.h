
#pragma once

#include "../../core/widget.h"

enum class ToolbarDirection
{
	Horizontal,
	Vertical
};

class ToolbarButton;

class Toolbar : public Widget
{
public:
	Toolbar(Widget* parent);
	~Toolbar();

	void SetDirection(ToolbarDirection direction);
	ToolbarDirection GetDirection(ToolbarDirection) const { return direction; }

	ToolbarButton* AddButton(std::string icon, std::string text, std::function<void()> onClicked = {});

protected:
	void OnGeometryChanged() override;

private:
	ToolbarDirection direction = ToolbarDirection::Horizontal;
	std::vector<ToolbarButton*> buttons;
};
