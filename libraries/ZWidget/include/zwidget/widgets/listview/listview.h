
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
	void OnMouseDown(const Point& pos, int key) override;
	void OnMouseDoubleclick(const Point& pos, int key) override;
	void OnKeyDown(EInputKey key) override;

	std::vector<std::string> items;
	int selectedItem = 0;
};
