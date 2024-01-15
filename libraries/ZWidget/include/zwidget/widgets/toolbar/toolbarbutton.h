
#pragma once

#include "../../core/widget.h"

class ToolbarButton : public Widget
{
public:
	ToolbarButton(Widget* parent);
	~ToolbarButton();

protected:
	void OnPaint(Canvas* canvas) override;
};
