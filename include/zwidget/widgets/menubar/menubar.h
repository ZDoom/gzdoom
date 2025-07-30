
#pragma once

#include "../../core/widget.h"
#include "../textlabel/textlabel.h"
#include "../imagebox/imagebox.h"
#include <vector>

class Menu;
class MenubarItem;
class MenuItem;
class MenuItemSeparator;

class Menubar : public Widget
{
public:
	Menubar(Widget* parent);
	~Menubar();

	MenubarItem* AddItem(std::string text, std::function<void(Menu* menu)> onOpen, bool alignRight = false);

protected:
	void OnGeometryChanged() override;
	bool OnMouseDown(const Point& pos, InputKey key) override;
	bool OnMouseUp(const Point& pos, InputKey key) override;
	void OnMouseMove(const Point& pos) override;
	void OnKeyDown(InputKey key) override;

private:
	void ShowMenu(MenubarItem* item);
	void CloseMenu();

	MenubarItem* GetMenubarItemAt(const Point& pos);
	int GetItemIndex(MenubarItem* item);

	std::vector<MenubarItem*> menuItems;
	int currentMenubarItem = -1;
	Menu* openMenu = nullptr;
	bool modalMode = false;

	friend class MenubarItem;
};

class MenubarItem : public Widget
{
public:
	MenubarItem(Menubar* menubar, std::string text, bool alignRight);

	void SetOpenCallback(std::function<void(Menu* menu)> callback) { onOpen = std::move(callback); }
	const std::function<void(Menu* menu)>& GetOpenCallback() const { return onOpen; }

	double GetPreferredWidth() const;

	bool AlignRight = false;

protected:
	void OnPaint(Canvas* canvas) override;
	bool OnMouseDown(const Point& pos, InputKey key) override;
	bool OnMouseUp(const Point& pos, InputKey key) override;
	void OnMouseMove(const Point& pos) override;
	void OnMouseLeave() override;

private:
	Menubar* menubar = nullptr;
	std::function<void(Menu* menu)> onOpen;
	std::string text;
};

class Menu : public Widget
{
public:
	Menu(Widget* parent);

	void SetLeftPosition(const Point& globalPos);
	void SetRightPosition(const Point& globalPos);
	MenuItem* AddItem(std::shared_ptr<Image> icon, std::string text, std::function<void()> onClick = {});
	MenuItemSeparator* AddSeparator();

	double GetPreferredWidth() const;
	double GetPreferredHeight() const;

	void SetSelected(MenuItem* item);

protected:
	void OnGeometryChanged() override;

	std::function<void()> onCloseMenu;
	MenuItem* selectedItem = nullptr;

	friend class Menubar;
};

class MenuItem : public Widget
{
public:
	MenuItem(Menu* menu, std::function<void()> onClick);

	void Click();

	Menu* menu = nullptr;
	ImageBox* icon = nullptr;
	TextLabel* text = nullptr;

protected:
	bool OnMouseUp(const Point& pos, InputKey key) override;
	void OnMouseMove(const Point& pos) override;
	void OnMouseLeave() override;
	void OnGeometryChanged() override;

private:
	std::function<void()> onClick;
};

class MenuItemSeparator : public Widget
{
public:
	MenuItemSeparator(Widget* parent);

protected:
	void OnPaint(Canvas* canvas) override;
};
