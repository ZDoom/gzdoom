#include "widgets/listview/listview.h"
#include "widgets/scrollbar/scrollbar.h"

ListView::ListView(Widget* parent) : Widget(parent)
{
	SetStyleClass("listview");

	scrollbar = new Scrollbar(this);
	scrollbar->FuncScroll = [this]() { OnScrollbarScroll(); };

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
	if (column < 0 || column >= (int)columnwidths.size())
		return;

	std::vector<std::string> newEntry;
	for (size_t i = 0u; i < columnwidths.size(); ++i)
		newEntry.push_back("");

	newEntry[column] = text;
	if (index >= 0)
	{
		if (index >= (int)items.size())
		{
			newEntry[column] = "";
			while ((int)items.size() < index)
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
	scrollbar->SetRanges(GetHeight(), items.size() * getItemHeight());
	Update();
}

void ListView::UpdateItem(const std::string& text, int index, int column)
{
	if (index < 0 || index >= (int)items.size() || column < 0 || column >= (int)columnwidths.size())
		return;

	items[index][column] = text;
	Update();
}

void ListView::RemoveItem(int index)
{
	if (!items.size() || index >= (int)items.size())
		return;

	if (index < 0)
		index = static_cast<int>(items.size()) - 1;

	if (selectedItem == index)
		SetSelectedItem(0);

	items.erase(items.begin() + index);
	scrollbar->SetRanges(GetHeight(), items.size() * getItemHeight());
	Update();
}

void ListView::Activate()
{
	if (OnActivated)
		OnActivated();
}

void ListView::SetSelectedItem(int index)
{
	if (selectedItem != index && index >= 0 && index < (int)items.size())
	{
		selectedItem = index;
		if (OnChanged) OnChanged(selectedItem);
		Update();
	}
}

double ListView::getItemHeight()
{
	return 20.0;
}

void ListView::ScrollToItem(int index)
{
	double itemHeight = getItemHeight();
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
	scrollbar->SetRanges(h, items.size() * getItemHeight());
}

void ListView::OnPaint(Canvas* canvas)
{
	double y = -scrollbar->GetPosition();
	double x = 2.0;
	double w = GetWidth() - scrollbar->GetPreferredWidth() - 2.0;
	double h = getItemHeight();

	Colorf textColor = GetStyleColor("color");
	Colorf selectionColor = GetStyleColor("selection-color");

	// Make sure the text doesn't enter the scrollbar's area.
	canvas->pushClip({ 0.0, 0.0, w, GetHeight() });

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

	canvas->popClip();
}

bool ListView::OnMouseDown(const Point& pos, InputKey key)
{
	SetFocus();

	if (key == InputKey::LeftMouse)
	{
		int index = (int)((pos.y - 5.0 + scrollbar->GetPosition()) / getItemHeight());
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
		scrollbar->SetPosition(std::max(scrollbar->GetPosition() - getItemHeight(), 0.0));
	}
	else if (key == InputKey::MouseWheelDown)
	{
		scrollbar->SetPosition(std::min(scrollbar->GetPosition() + getItemHeight(), scrollbar->GetMax()));
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
	else if (key == InputKey::Home)
	{
		if (selectedItem > 0)
		{
			SetSelectedItem(0);
		}
		ScrollToItem(selectedItem);
	}
	else if (key == InputKey::End)
	{
		if (selectedItem + 1 < (int)items.size())
		{
			SetSelectedItem((int)items.size() - 1);
		}
		ScrollToItem(selectedItem);
	}
	else if (key == InputKey::PageUp)
	{
		double h = GetHeight();
		if (h <= 0.0 || items.empty())
			return;
		int itemsPerPage = (int)std::max(std::round(h / getItemHeight()), 1.0);
		int nextItem = std::max(selectedItem - itemsPerPage, 0);
		if (nextItem != selectedItem)
		{
			SetSelectedItem(nextItem);
			ScrollToItem(selectedItem);
		}
	}
	else if (key == InputKey::PageDown)
	{
		double h = GetHeight();
		if (h <= 0.0 || items.empty())
			return;
		int itemsPerPage = (int)std::max(std::round(h / getItemHeight()), 1.0);
		int prevItem = std::min(selectedItem + itemsPerPage, (int)items.size() - 1);
		if (prevItem != selectedItem)
		{
			SetSelectedItem(prevItem);
			ScrollToItem(selectedItem);
		}
	}
}

double ListView::GetPreferredWidth()
{
	double total = 0.0;

	for (double width : columnwidths)
	{
		if (width > 0.0) total += width;
	}

	if (total == 0.0)
	{
		auto canvas = GetCanvas();

		for (const auto& row : items)
		{
			double wRow = 0.0;
			for (const auto& cell : row)
			{
				wRow += canvas->measureText(cell).width;
			}
			if (wRow > total) total = wRow;
		}
	}

	return total + 10.0*2 + scrollbar->GetPreferredWidth();
}

double ListView::GetPreferredHeight()
{
	return items.size()*20.0 + 10.0*2; // Items plus top/bottom padding
}

double ListView::GetMinimumHeight() const
{
	return 20.0 + 10.0*2; // One item plus top/bottom padding
}
