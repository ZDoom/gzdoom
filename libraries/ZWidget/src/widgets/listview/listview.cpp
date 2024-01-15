
#include "widgets/listview/listview.h"
#include "widgets/scrollbar/scrollbar.h"

ListView::ListView(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(10.0, 10.0, 3.0, 10.0);

	scrollbar = new Scrollbar(this);
	scrollbar->FuncScroll = [=]() { OnScrollbarScroll(); };
}

void ListView::AddItem(const std::string& text)
{
	items.push_back(text);
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
	for (const std::string& item : items)
	{
		double itemY = y;
		if (itemY + h >= 0.0 && itemY < GetHeight())
		{
			if (index == selectedItem)
			{
				canvas->fillRect(Rect::xywh(x - 2.0, itemY, w, h), Colorf::fromRgba8(100, 100, 100));
			}
			canvas->drawText(Point(x, y + 15.0), Colorf::fromRgba8(255, 255, 255), item);
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
