#include <algorithm>

#include "widgets/dropdown/dropdown.h"
#include "core/widget.h"

Dropdown::Dropdown(Widget* parent) : Widget(parent)
{
	SetStyleClass("dropdown");
	SetFocus();

	// Subscribe to all parent widgets to receive notifications.
	for (Widget* p = Parent(); p; p = p->Parent())
		p->Subscribe(this);
}

DropdownList::DropdownList(Widget* parent, Dropdown* owner) : ListView(parent), owner(owner)
{}

void DropdownList::OnKeyDown(InputKey key)
{
	owner->OnKeyDown(key);
}

void Dropdown::Notify(Widget* source, const WidgetEvent type)
{
	if (type != WidgetEvent::VisibilityChange) return;
	if (!dropdownOpen) return;

	for (Widget* p = this; p != nullptr; p = p->Parent())
	{
		if (p->IsHidden())
		{
			CloseDropdown();
			return;
		}
	}
}

void Dropdown::ItemsChanged()
{
	if (!CloseDropdown())
	{
		Update();
	}
}

void Dropdown::AddItem(const std::string& text, int index)
{
	if (index < 0)
	{
		items.push_back(text);
	}
	else
	{
		items.insert(items.begin() + index, text);
	}

	if (selectedItem == -1 && !items.empty())
		SetSelectedItem(0);

	ItemsChanged();
}

bool Dropdown::UpdateItem(const std::string& text, int index)
{
	if (!items.size() || index >= (int)items.size())
		return false;

	if (index < 0)
		index = static_cast<int>(items.size()) - 1;

	items[index] = text;

	if (selectedItem == index)
	{
		this->text = text;
	}

	ItemsChanged();

	return true;
}

const std::string & Dropdown::GetItem(int index)
{
	return items[index];
}

bool Dropdown::RemoveItem(int index)
{
	if (!items.size() || index >= (int)items.size())
		return false;

	if (index < 0)
		index = static_cast<int>(items.size()) - 1;

	items.erase(items.begin() + index);

	if ((int)items.size() == 0)
	{
		SetSelectedItem(-1);
	}
	else if (selectedItem == index)
	{
		SetSelectedItem(0);
	}
	else if (selectedItem > index)
	{
		SetSelectedItem(selectedItem - 1);
	}

	ItemsChanged();

	return true;
}

void Dropdown::ClearItems()
{
	items.clear();
	SetSelectedItem(-1);
	ItemsChanged();
}

void Dropdown::SetSelectedItem(int index)
{
	if (index < 0) index = 0;
	if (index >= (int)items.size()) index = items.size() - 1;

	if (selectedItem == index) return;

	selectedItem = index;
	text = (selectedItem != -1) ? items[selectedItem] : "";

	if (OnChanged) OnChanged(selectedItem);

	if (dropdownOpen) listView->ScrollToItem(listView->selectedItem = selectedItem);

	Update();
}

double Dropdown::GetPreferredHeight() const
{
	return 20.0 + 2*5.0; // Text plus top/bottom padding
}

double Dropdown::GetPreferredWidth() const
{
	Canvas* canvas = GetCanvas();

	double maxWidth = 0.0;
	for (const auto& item : items)
	{
		auto width = canvas->measureText(item).width;
		if (width > maxWidth)
		{
			maxWidth = width;
		}
	}

	return maxWidth + 2*10.0 + 16.0; // content, pad l/r, scrollbar from dropdown
}

void Dropdown::OnPaint(Canvas* canvas)
{
	Colorf textColor = GetStyleColor("color");
	Colorf arrowColor = GetStyleColor("arrow-color");

	double w = GetWidth();
	double h = GetHeight();

	auto vtp = canvas->verticalTextAlign();
	double textY = (h-(vtp.bottom-vtp.top))/2.0 + vtp.baseline;
	canvas->drawText(Point(7.0, textY), textColor, text);

	double arrowS = 8.0;
	double arrowX = w - 3.0; // rightmost point, aligned with scrollbar
	double arrowY = (h-arrowS)/2;

	Point p1(arrowX, arrowY);
	Point p2(arrowX - arrowS, arrowY);
	Point p3(arrowX - arrowS/2, arrowY + arrowS);

	canvas->line(p1, p2, arrowColor);
	canvas->line(p2, p3, arrowColor);
	canvas->line(p3, p1, arrowColor);
}

bool Dropdown::OnMouseDown(const Point& pos, InputKey key)
{
	if (key == InputKey::LeftMouse)
	{
		if (dropdownOpen)
		{
			CloseDropdown();
			SetFocus();
			return true;
		}
		else
		{
			SetFocus();
			OpenDropdown();
			return true;
		}
	}
	return false;
}

void Dropdown::OnKeyDown(InputKey key)
{
	switch (key)
	{
	case InputKey::Enter:
	case InputKey::Space:
		if (dropdownOpen)
			CloseDropdown();
		else
			OpenDropdown();
		break;

	case InputKey::Up:
		SetSelectedItem(selectedItem - 1);
		break;

	case InputKey::Down:
		SetSelectedItem(selectedItem + 1);
		break;

	case InputKey::Home:
		SetSelectedItem(0);
		break;

	case InputKey::End:
		SetSelectedItem((int)items.size());
		break;

	case InputKey::PageUp:
		SetSelectedItem(selectedItem - (maxDisplayItems? maxDisplayItems: (int)items.size()));
		break;

	case InputKey::PageDown:
		SetSelectedItem(selectedItem + (maxDisplayItems? maxDisplayItems: (int)items.size()));
		break;

	default:
		break;
	}
}

void Dropdown::SetMaxDisplayItems(int items)
{
	maxDisplayItems = std::max<int>(0, items);
}

size_t Dropdown::GetDisplayItems()
{
	return (maxDisplayItems < 1)
		? std::max<size_t>(items.size(), 1ul)
		: std::min<size_t>(items.size(), (size_t)maxDisplayItems);
}

void Dropdown::SetDropdownDirection(bool below)
{
	dropdownDirection = below;
}

void Dropdown::OnGeometryChanged()
{
	if (dropdownOpen)
	{
		Point pos = MapTo(Window(), Point(0,0));

		double width = GetWidth() + GetNoncontentLeft() + GetNoncontentRight();
		double innerH = GetDisplayItems() * 25.0 + 10.0;
		double outerH = GetHeight();

		pos.x -= GetNoncontentLeft();

		// adjust to be above/below box
		if (dropdownDirection)
			pos.y += outerH + GetNoncontentBottom() - 1.0;
		else
			pos.y -= innerH + GetNoncontentTop() - 1.0;

		dropdown->SetFrameGeometry(pos.x, pos.y, width, innerH);
	}
}

void Dropdown::OnLostFocus()
{
	if (!dropdownOpen) return;

	// Check if the mouse is over the dropdown itself or one of its children
	auto hover = Window()->HoverWidget;
	if (hover && (hover == dropdown || dropdown->IsParent(hover))) return;

	CloseDropdown();
}

bool Dropdown::OpenDropdown()
{
	if (dropdownOpen || items.empty()) return false;

	dropdownOpen = true;

	dropdown = new Widget(Window());
	listView = new DropdownList(dropdown, this);
	for (const auto& item : items)
	{
		listView->AddItem(item);
	}

	listView->SetSelectedItem(selectedItem);

	listView->OnActivated = [=]() { OnDropdownActivated(); };
	listView->OnChanged = [=](int index) { OnDropdownActivated(); };

	listView->SetFrameGeometry(
		0,
		0,
		GetWidth() + GetNoncontentLeft() + GetNoncontentRight(),
		GetDisplayItems() * 25.0 + 10.0
	);
	OnGeometryChanged();

	dropdown->Show();

	listView->ScrollToItem(selectedItem);

	return true;
}

bool Dropdown::CloseDropdown()
{
	if (!dropdownOpen || !dropdown) return false;

	dropdown->Close();
	dropdown = nullptr;
	listView = nullptr;
	dropdownOpen = false;

	Update();

	return true;
}

void Dropdown::OnDropdownActivated()
{
	if (listView)
	{
		SetSelectedItem(listView->GetSelectedItem());
	}
	CloseDropdown();
	SetFocus();
}
