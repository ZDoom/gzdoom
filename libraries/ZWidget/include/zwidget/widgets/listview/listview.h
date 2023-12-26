
#pragma once

#include "../../core/widget.h"
#include <vector>

class ListView : public Widget
{
public:
	ListView(Widget* parent = nullptr);

	void AddItem(const std::string& text);

protected:
	void OnPaint(Canvas* canvas) override;
	void OnPaintFrame(Canvas* canvas) override;

	std::vector<std::string> items;
};
