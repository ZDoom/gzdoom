
#pragma once

#include "../../core/widget.h"
#include <vector>
#include <functional>

class Scrollbar;

class ListView : public Widget
{
public:
	ListView(Widget* parent = nullptr);

	void SetColumnWidths(const std::vector<double>& widths);
	void AddItem(const std::string& text, int index = -1, int column = 0);
	void UpdateItem(const std::string& text, int index, int column = 0);
	void RemoveItem(int index = -1);
	size_t GetItemAmount() const { return items.size(); }
	size_t GetColumnAmount() const { return columnwidths.size(); }
	int GetSelectedItem() const { return selectedItem; }
	void SetSelectedItem(int index);
	void ScrollToItem(int index);

	void Activate();

	std::function<void(int)> OnChanged;
	std::function<void()> OnActivated;

protected:
	void OnPaint(Canvas* canvas) override;
	void OnPaintFrame(Canvas* canvas) override;
	bool OnMouseDown(const Point& pos, int key) override;
	bool OnMouseDoubleclick(const Point& pos, int key) override;
	bool OnMouseWheel(const Point& pos, EInputKey key) override;
	void OnKeyDown(EInputKey key) override;
	void OnGeometryChanged() override;
	void OnScrollbarScroll();

	Scrollbar* scrollbar = nullptr;

	std::vector<std::vector<std::string>> items;
	std::vector<double> columnwidths;
	int selectedItem = 0;
};
