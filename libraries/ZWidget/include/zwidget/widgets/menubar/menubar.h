
#pragma once

#include "../../core/widget.h"

class Menubar : public Widget
{
public:
	Menubar(Widget* parent);
	~Menubar();

protected:
	void OnPaint(Canvas* canvas) override;
};
