
#include "widgets/listview/listview.h"
#include "widgets/scrollbar/scrollbar.h"

ListView::ListView(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(10.0, 10.0, 3.0, 10.0);

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
			column.erase(column.end());
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

	int index = 0;
	for (const std::vector<std::string>& item : items)
	{
		double itemY = y;
		if (itemY + h >= 0.0 && itemY < GetHeight())
		{
			if (index == selectedItem)
			{
				canvas->fillRect(Rect::xywh(x - 2.0, itemY, w, h), Colorf::fromRgba8(100, 100, 100));
			}
			double cx = x;
			for (size_t entry = 0u; entry < item.size(); ++entry)
			{
				canvas->drawText(Point(cx, y + 15.0), Colorf::fromRgba8(255, 255, 255), item[entry]);
				cx += columnwidths[entry];
			}
		}
		y += h;
		index++;
	}
}

void ListView::OnPaintFrame(Canvas* canvas)
{
	double w = GetFrameGeometry().width;
	double h = GetFrameGeometry().height;
	Colorf bordercolor = Colorf::fromRgba8(100, 100, 100);
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, h), Colorf::fromRgba8(38, 38, 38));
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, 1.0), bordercolor);
	canvas->fillRect(Rect::xywh(0.0, h - 1.0, w, 1.0), bordercolor);
	canvas->fillRect(Rect::xywh(0.0, 0.0, 1.0, h - 0.0), bordercolor);
	canvas->fillRect(Rect::xywh(w - 1.0, 0.0, 1.0, h - 0.0), bordercolor);
}

bool ListView::OnMouseDown(const Point& pos, int key)
{
	SetFocus();

	if (key == IK_LeftMouse)
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

bool ListView::OnMouseDoubleclick(const Point& pos, int key)
{
	if (key == IK_LeftMouse)
	{
		Activate();
	}
	return true;
}

bool ListView::OnMouseWheel(const Point& pos, EInputKey key)
{
	if (key == IK_MouseWheelUp)
	{
		scrollbar->SetPosition(std::max(scrollbar->GetPosition() - 20.0, 0.0));
	}
	else if (key == IK_MouseWheelDown)
	{
		scrollbar->SetPosition(std::min(scrollbar->GetPosition() + 20.0, scrollbar->GetMax()));
	}
	return true;
}

void ListView::OnKeyDown(EInputKey key)
{
	if (key == IK_Down)
	{
		if (selectedItem + 1 < (int)items.size())
		{
			SetSelectedItem(selectedItem + 1);
		}
		ScrollToItem(selectedItem);
	}
	else if (key == IK_Up)
	{
		if (selectedItem > 0)
		{
			SetSelectedItem(selectedItem - 1);
		}
		ScrollToItem(selectedItem);
	}
	else if (key == IK_Enter)
	{
		Activate();
	}
}
