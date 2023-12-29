
#include "widgets/listview/listview.h"

ListView::ListView(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(10.0, 5.0, 10.0, 5.0);
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

void ListView::OnPaint(Canvas* canvas)
{
	double y = 20.0;
	double x = 2.0;
	double w = GetFrameGeometry().width;
	double h = 20.0;

	int index = 0;
	for (const std::string& item : items)
	{
		if (index == selectedItem)
		{
			canvas->fillRect(Rect::xywh(x - 2.0, y + 5.0 - h, w, h), Colorf::fromRgba8(100, 100, 100));
		}
		canvas->drawText(Point(x, y), Colorf::fromRgba8(255, 255, 255), item);
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

void ListView::OnMouseDown(const Point& pos, int key)
{
	SetFocus();

	if (key == IK_LeftMouse)
	{
		int index = (int)((pos.y - 5.0) / 20.0);
		if (index >= 0 && (size_t)index < items.size())
		{
			selectedItem = index;
			Update();
		}
	}
}

void ListView::OnMouseDoubleclick(const Point& pos, int key)
{
	if (key == IK_LeftMouse)
	{
		Activate();
	}
}

void ListView::OnKeyDown(EInputKey key)
{
	if (key == IK_Down)
	{
		if (selectedItem + 1 < (int)items.size())
		{
			selectedItem++;
			Update();
		}
	}
	else if (key == IK_Up)
	{
		if (selectedItem > 0)
		{
			selectedItem--;
			Update();
		}
	}
	else if (key == IK_Enter)
	{
		Activate();
	}
}
