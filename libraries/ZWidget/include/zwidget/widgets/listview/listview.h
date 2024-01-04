
#pragma once

#include "../../core/widget.h"
#include <vector>
#include <functional>

class Scrollbar;

class ListView : public Widget
{
public:
	ListView(Widget* parent = nullptr);

	void AddItem(const std::string& text);
	int GetSelectedItem() const { return selectedItem; }
	void ScrollToItem(int index);

	void Activate();

	std::function<void()> OnActivated;

protected:
	void OnPaint(Canvas* canvas) override;
	void OnPaintFrame(Canvas* canvas) override;
	void OnMouseDown(const Point& pos, int key) override;
	void OnMouseDoubleclick(const Point& pos, int key) override;
	void OnMouseWheel(const Point& pos, EInputKey key) override;
	void OnKeyDown(EInputKey key) override;
	void OnGeometryChanged() override;
	void OnScrollbarScroll();

	Scrollbar* scrollbar = nullptr;

	std::vector<std::string> items;
	int selectedItem = 0;
};
