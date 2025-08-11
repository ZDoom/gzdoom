
#include "widgets/menubar/menubar.h"
#include "core/colorf.h"

Menubar::Menubar(Widget* parent) : Widget(parent)
{
	SetStyleClass("menubar");
}

Menubar::~Menubar()
{
}

MenubarItem* Menubar::AddItem(std::string text, std::function<void(Menu* menu)> onOpen, bool alignRight)
{
	auto item = new MenubarItem(this, text, alignRight);
	item->SetOpenCallback(std::move(onOpen));
	menuItems.push_back(item);
	OnGeometryChanged();
	return item;
}

void Menubar::ShowMenu(MenubarItem* item)
{
	int index = GetItemIndex(item);
	if (index == currentMenubarItem)
		return;

	CloseMenu();
	SetFocus();
	SetModalCapture();
	currentMenubarItem = index;
	if (currentMenubarItem != -1)
		menuItems[currentMenubarItem]->SetStyleState("hover");
	modalMode = true;
	if (item->GetOpenCallback())
	{
		openMenu = new Menu(this);
		openMenu->onCloseMenu = [=]() { CloseMenu(); };
		item->GetOpenCallback()(openMenu);
		if (item->AlignRight)
			openMenu->SetRightPosition(item->MapToGlobal(Point(item->GetWidth(), item->GetHeight())));
		else
			openMenu->SetLeftPosition(item->MapToGlobal(Point(0.0, item->GetHeight())));
		openMenu->Show();
	}
}

void Menubar::CloseMenu()
{
	if (currentMenubarItem != -1)
		menuItems[currentMenubarItem]->SetStyleState("");
	currentMenubarItem = -1;
	delete openMenu;
	openMenu = nullptr;
	ReleaseModalCapture();
	modalMode = false;
}

void Menubar::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();
	double left = 0.0;
	double right = w;
	for (MenubarItem* item : menuItems)
	{
		double itemwidth = item->GetPreferredWidth();
		itemwidth += 16.0;
		if (!item->AlignRight)
		{
			item->SetFrameGeometry(left, 0.0, itemwidth, h);
			left += itemwidth;
		}
		else
		{
			right -= itemwidth;
			item->SetFrameGeometry(right, 0.0, itemwidth, h);
		}
	}
}

MenubarItem* Menubar::GetMenubarItemAt(const Point& pos)
{
	Widget* widget = ChildAt(pos);
	for (MenubarItem* item : menuItems)
	{
		if (widget == item)
			return item;
	}
	return nullptr;
}

bool Menubar::OnMouseDown(const Point& pos, InputKey key)
{
	if (!modalMode)
		return Widget::OnMouseDown(pos, key);

	MenubarItem* item = GetMenubarItemAt(pos);
	if (item)
		ShowMenu(item);
	else
		CloseMenu();
	return true;
}

bool Menubar::OnMouseUp(const Point& pos, InputKey key)
{
	if (!modalMode)
		return Widget::OnMouseUp(pos, key);

	MenubarItem* item = GetMenubarItemAt(pos);
	if (!item)
		CloseMenu();
	return true;
}

void Menubar::OnMouseMove(const Point& pos)
{
	if (!modalMode)
		return Widget::OnMouseMove(pos);

	MenubarItem* item = GetMenubarItemAt(pos);
	if (item)
		ShowMenu(item);
}

int Menubar::GetItemIndex(MenubarItem* item)
{
	int i = 0;
	for (MenubarItem* cur : menuItems)
	{
		if (cur == item)
			return i;
		i++;
	}
	return -1;
}

void Menubar::OnKeyDown(InputKey key)
{
	if (!modalMode)
		return Widget::OnKeyDown(key);

	if (key == InputKey::Left)
	{
		if (!menuItems.empty())
		{
			int index = currentMenubarItem - 1;
			if (index < 0)
				index = (int)menuItems.size() - 1;
			ShowMenu(menuItems[index]);
		}
	}
	else if (key == InputKey::Right)
	{
		if (!menuItems.empty())
		{
			int index = currentMenubarItem + 1;
			if (index == (int)menuItems.size())
				index = 0;
			ShowMenu(menuItems[index]);
		}
	}
	else if (key == InputKey::Up)
	{
		if (openMenu)
		{
			// Keep trying until we don't find a separator
			for (int i = 0; i < 10; i++)
			{
				if (!openMenu->selectedItem || !openMenu->selectedItem->PrevSibling())
				{
					if (openMenu->LastChild())
						openMenu->SetSelected(static_cast<MenuItem*>(openMenu->LastChild()));
				}
				else
				{
					openMenu->SetSelected(static_cast<MenuItem*>(openMenu->selectedItem->PrevSibling()));
				}

				if (openMenu->selectedItem && openMenu->selectedItem->GetStyleClass() != "menuitemseparator")
					break;
			}
		}
	}
	else if (key == InputKey::Down)
	{
		if (openMenu)
		{
			// Keep trying until we don't find a separator
			for (int i = 0; i < 10; i++)
			{
				if (!openMenu->selectedItem || !openMenu->selectedItem->NextSibling())
				{
					if (openMenu->FirstChild())
						openMenu->SetSelected(static_cast<MenuItem*>(openMenu->FirstChild()));
				}
				else
				{
					openMenu->SetSelected(static_cast<MenuItem*>(openMenu->selectedItem->NextSibling()));
				}

				if (openMenu->selectedItem && openMenu->selectedItem->GetStyleClass() != "menuitemseparator")
					break;
			}
		}
	}
	else if (key == InputKey::Enter)
	{
		if (openMenu && openMenu->selectedItem)
			openMenu->selectedItem->Click();
	}
}

/////////////////////////////////////////////////////////////////////////////

MenubarItem::MenubarItem(Menubar* menubar, std::string text, bool alignRight) : Widget(menubar), menubar(menubar), text(text), AlignRight(alignRight)
{
	SetStyleClass("menubaritem");
}

bool MenubarItem::OnMouseDown(const Point& pos, InputKey key)
{
	menubar->ShowMenu(this);
	return true;
}

bool MenubarItem::OnMouseUp(const Point& pos, InputKey key)
{
	return true;
}

void MenubarItem::OnMouseMove(const Point& pos)
{
	if (GetStyleState().empty())
	{
		SetStyleState("hover");
	}
}

void MenubarItem::OnMouseLeave()
{
	SetStyleState("");
}

double MenubarItem::GetPreferredWidth() const
{
	Canvas* canvas = GetCanvas();
	return canvas->measureText(text).width;
}

void MenubarItem::OnPaint(Canvas* canvas)
{
	double x = (GetWidth() - canvas->measureText(text).width) * 0.5;
	canvas->drawText(Point(x, 21.0), GetStyleColor("color"), text);
}

/////////////////////////////////////////////////////////////////////////////

Menu::Menu(Widget* parent) : Widget(parent, WidgetType::Popup)
{
	SetStyleClass("menu");
}

void Menu::SetLeftPosition(const Point& pos)
{
	SetFrameGeometry(Rect::xywh(pos.x, pos.y, GetPreferredWidth() + GetNoncontentLeft() + GetNoncontentRight(), GetPreferredHeight() + GetNoncontentTop() + GetNoncontentBottom()));
}

void Menu::SetRightPosition(const Point& pos)
{
	SetFrameGeometry(Rect::xywh(pos.x - GetWidth() - GetNoncontentLeft() - GetNoncontentRight(), pos.y, GetWidth() + GetNoncontentLeft() + GetNoncontentRight(), GetHeight() + GetNoncontentTop() + GetNoncontentBottom()));
}

MenuItem* Menu::AddItem(std::shared_ptr<Image> icon, std::string text, std::function<void()> onClick)
{
	auto item = new MenuItem(this, [this, onClick]() { if (onCloseMenu) onCloseMenu(); if (onClick) onClick(); });
	if (icon)
		item->icon->SetImage(icon);
	item->text->SetText(text);
	return item;
}

MenuItemSeparator* Menu::AddSeparator()
{
	auto sep = new MenuItemSeparator(this);
	return sep;
}

double Menu::GetPreferredWidth() const
{
	return GridFitSize(200.0);
}

double Menu::GetPreferredHeight() const
{
	double h = 0.0;
	for (Widget* item = FirstChild(); item != nullptr; item = item->NextSibling())
	{
		double itemheight = GridFitSize((item->GetStyleClass() == "menuitemseparator") ? 7.0 : 30.0);
		h += itemheight;
	}
	return h;
}

void Menu::OnGeometryChanged()
{
	double w = GetWidth();
	double y = 0.0;
	for (Widget* item = FirstChild(); item != nullptr; item = item->NextSibling())
	{
		double itemheight = GridFitSize((item->GetStyleClass() == "menuitemseparator") ? 7.0 : 30.0);
		item->SetFrameGeometry(Rect::xywh(0.0, y, w, itemheight));
		y += itemheight;
	}
}

void Menu::SetSelected(MenuItem* item)
{
	if (selectedItem)
	{
		selectedItem->SetStyleState("");
	}
	selectedItem = item;
	if (selectedItem)
	{
		selectedItem->SetStyleState("hover");
	}
}

/////////////////////////////////////////////////////////////////////////////

MenuItem::MenuItem(Menu* menu, std::function<void()> onClick) : Widget(menu), menu(menu), onClick(onClick)
{
	SetStyleClass("menuitem");
	icon = new ImageBox(this);
	text = new TextLabel(this);
}

void MenuItem::OnMouseMove(const Point& pos)
{
	menu->SetSelected(this);
}

bool MenuItem::OnMouseUp(const Point& pos, InputKey key)
{
	Click();
	return true;
}

void MenuItem::Click()
{
	if (onClick)
	{
		// We have to make a copy of the handler as it may delete 'this'
		auto handler = onClick;
		handler();
	}
}

void MenuItem::OnMouseLeave()
{
	menu->SetSelected(nullptr);
}

void MenuItem::OnGeometryChanged()
{
	double iconwidth = GridFitSize(icon->GetPreferredWidth());
	double iconheight = GridFitSize(icon->GetPreferredHeight());
	double w = GetWidth();
	double h = GetHeight();
	double textheight = 19.0;
	double x0 = GridFitPoint(5.0);
	double x1 = GridFitPoint(5.0 + iconwidth);
	icon->SetFrameGeometry(Rect::xywh(x0, GridFitPoint((h - iconheight) * 0.5), iconwidth, iconheight));
	text->SetFrameGeometry(Rect::xywh(x1, GridFitPoint((h - textheight) * 0.5), w - x1, textheight));
}

/////////////////////////////////////////////////////////////////////////////

MenuItemSeparator::MenuItemSeparator(Widget* parent) : Widget(parent)
{
	SetStyleClass("menuitemseparator");
}

void MenuItemSeparator::OnPaint(Canvas* canvas)
{
	canvas->fillRect(Rect::xywh(0.0, GridFitPoint(GetHeight() * 0.5), GetWidth(), GridFitSize(1.0)), Colorf::fromRgba8(75, 75, 75));
}
