#pragma once

#include <string>
#include <vector>
#include <functional>

#include "../../core/widget.h"
#include "../../widgets/listview/listview.h"

class DropdownList : public ListView
{
public:
	DropdownList(Widget* parent, Dropdown* owner);
protected:
	void OnKeyDown(InputKey key) override;
	Dropdown* owner;
};

class Dropdown : public Widget
{
public:
	Dropdown(Widget* parent);

	void AddItem(const std::string& text, int index = -1);
	bool UpdateItem(const std::string& text, int index);
	bool RemoveItem(int index = -1);
	void ClearItems();
	const std::string& GetItem(int index);

	void SetMaxDisplayItems(int items);
	void SetDropdownDirection(bool below);

	int GetSelectedItem() const { return selectedItem; }
	void SetSelectedItem(int index);

	double GetPreferredHeight() const;
	double GetPreferredWidth() const;

	std::function<void(int)> OnChanged;

protected:
	void OnPaint(Canvas* canvas) override;
	bool OnMouseDown(const Point& pos, InputKey key) override;
	void OnKeyDown(InputKey key) override;
	void OnGeometryChanged() override;
	void OnLostFocus() override;
	void Notify(Widget* source, const WidgetEvent type) override;

private:
	void ItemsChanged();
	bool OpenDropdown();
	bool CloseDropdown();
	void OnDropdownActivated();

	size_t GetDisplayItems();

	std::vector<std::string> items;
	int selectedItem = -1;
	std::string text;

	bool dropdownOpen = false;
	Widget* dropdown = nullptr;
	DropdownList* listView = nullptr;

	int maxDisplayItems = 0;
	bool dropdownDirection = true;

	friend DropdownList;
};
