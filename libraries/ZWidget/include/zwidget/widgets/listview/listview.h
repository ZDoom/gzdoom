
#pragma once

#include "../../core/widget.h"
#include <vector>
#include <functional>

class ListView : public Widget
{
public:
	ListView(Widget* parent = nullptr);

	void AddItem(const std::string& text);
	int GetSelectedItem() const { return selectedItem; }

	void Activate();

	std::function<void()> OnActivated;

protected:
	void OnPaint(Canvas* canvas) override;
	void OnPaintFrame(Canvas* canvas) override;
	void OnMouseDown(const Point& pos, int key) override;
	void OnMouseDoubleclick(const Point& pos, int key) override;
	void OnKeyDown(EInputKey key) override;

	std::vector<std::string> items;
	int selectedItem = 0;
};
