
#include "widgets/listview/listview.h"
#include "widgets/scrollbar/scrollbar.h"

ListView::ListView(Widget* parent) : Widget(parent)
{
	SetStyleClass("listview");

	scrollbar = new Scrollbar(this);
	scrollbar->FuncScroll = [=]() { OnScrollbarScroll(); };

	SetColumnWidths({ 0.0 });
}

void ListView::SetColumnWidths(const std::vector<double>& widths)
{
	columnwidths = widths;

	bool updated = false;
	const size_t newWidth = columnwidths.size();
	for (std::vector<std::string>& column : items)
	{
		while (column.size() < newWidth)
		{
			updated = true;
			column.push_back("");
		}
		while (column.size() > newWidth)
		{
			updated = true;
			column.pop_back();
		}
	}

	if (updated)
		Update();
}

void ListView::AddItem(const std::string& text, int index, int column)
{
	if (column < 0 || column >= columnwidths.size())
		return;

	std::vector<std::string> newEntry;
	for (size_t i = 0u; i < columnwidths.size(); ++i)
		newEntry.push_back("");

	newEntry[column] = text;
	if (index >= 0)
	{
		if (index >= items.size())
		{
			newEntry[column] = "";
			while (items.size() < index)
				items.push_back(newEntry);

			newEntry[column] = text;
			items.push_back(newEntry);
		}
		else
		{
			items.insert(items.begin() + index, newEntry);
		}
	}
	else
	{
		items.push_back(newEntry);
	}
	scrollbar->SetRanges(GetHeight(), items.size() * 20.0);
	Update();
}

void ListView::UpdateItem(const std::string& text, int index, int column)
{
	if (index < 0 || index >= items.size() || column < 0 || column >= columnwidths.size())
		return;

	items[index][column] = text;
	Update();
}

void ListView::RemoveItem(int index)
{
	if (!items.size() || index >= items.size())
		return;

	if (index < 0)
		index = items.size() - 1;

	if (selectedItem == index)
		SetSelectedItem(0);

	items.erase(items.begin() + index);
	scrollbar->SetRanges(GetHeight(), items.size() * 20.0);
	Update();
}

void ListView::Activate()
{
	if (OnActivated)
		OnActivated();
}

void ListView::SetSelectedItem(int index)
{
	if (selectedItem != index && index >= 0 && index < items.size())
	{
		selectedItem = index;
		if (OnChanged) OnChanged(selectedItem);
		Update();
	}
}

void ListView::ScrollToItem(int index)
{
	double itemHeight = 20.0;
	double y = itemHeight * index;
	if (y < scrollbar->GetPosition())
	{
		scrollbar->SetPosition(y);
	}
	else if (y + itemHeight > scrollbar->GetPosition() + GetHeight())
	{
		scrollbar->SetPosition(std::max(y + itemHeight - GetHeight(), 0.0));
	}
}

void ListView::OnScrollbarScroll()
{
	Update();
}

void ListView::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();
	double sw = scrollbar->GetPreferredWidth();
	scrollbar->SetFrameGeometry(Rect::xywh(w - sw, 0.0, sw, h));
	scrollbar->SetRanges(h, items.size() * 20.0);
}

void ListView::OnPaint(Canvas* canvas)
{
	double y = -scrollbar->GetPosition();
	double x = 2.0;
	double w = GetWidth() - scrollbar->GetPreferredWidth() - 2.0;
	double h = 20.0;

	Colorf textColor = GetStyleColor("color");
	Colorf selectionColor = GetStyleColor("selection-color");

	int index = 0;
	for (const std::vector<std::string>& item : items)
	{
		double itemY = y;
		if (itemY + h >= 0.0 && itemY < GetHeight())
		{
			if (index == selectedItem)
			{
				canvas->fillRect(Rect::xywh(x - 2.0, itemY, w, h), selectionColor);
			}
			double cx = x;
			for (size_t entry = 0u; entry < item.size(); ++entry)
			{
				canvas->drawText(Point(cx, y + 15.0), textColor, item[entry]);
				cx += columnwidths[entry];
			}
		}
		y += h;
		index++;
	}
}

bool ListView::OnMouseDown(const Point& pos, InputKey key)
{
	SetFocus();

	if (key == InputKey::LeftMouse)
	{
		int index = (int)((pos.y - 5.0 + scrollbar->GetPosition()) / 20.0);
		if (index >= 0 && (size_t)index < items.size())
		{
			SetSelectedItem(index);
			ScrollToItem(selectedItem);
		}
	}
	return true;
}

bool ListView::OnMouseDoubleclick(const Point& pos, InputKey key)
{
	if (key == InputKey::LeftMouse)
	{
		Activate();
	}
	return true;
}

bool ListView::OnMouseWheel(const Point& pos, InputKey key)
{
	if (key == InputKey::MouseWheelUp)
	{
		scrollbar->SetPosition(std::max(scrollbar->GetPosition() - 20.0, 0.0));
	}
	else if (key == InputKey::MouseWheelDown)
	{
		scrollbar->SetPosition(std::min(scrollbar->GetPosition() + 20.0, scrollbar->GetMax()));
	}
	return true;
}

void ListView::OnKeyDown(InputKey key)
{
	if (key == InputKey::Down)
	{
		if (selectedItem + 1 < (int)items.size())
		{
			SetSelectedItem(selectedItem + 1);
		}
		ScrollToItem(selectedItem);
	}
	else if (key == InputKey::Up)
	{
		if (selectedItem > 0)
		{
			SetSelectedItem(selectedItem - 1);
		}
		ScrollToItem(selectedItem);
	}
	else if (key == InputKey::Enter)
	{
		Activate();
	}
}
