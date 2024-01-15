
#include "widgets/pushbutton/pushbutton.h"

PushButton::PushButton(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(10.0, 5.0, 10.0, 5.0);
}

void PushButton::SetText(const std::string& value)
{
	if (text != value)
	{
		text = value;
		Update();
	}
}

const std::string& PushButton::GetText() const
{
	return text;
}

double PushButton::GetPreferredHeight() const
{
	return 30.0;
}

void PushButton::OnPaintFrame(Canvas* canvas)
{
	double w = GetFrameGeometry().width;
	double h = GetFrameGeometry().height;
	Colorf bordercolor = Colorf::fromRgba8(100, 100, 100);
	Colorf buttoncolor = Colorf::fromRgba8(68, 68, 68);
	if (buttonDown)
		buttoncolor = Colorf::fromRgba8(88, 88, 88);
	else if (hot)
		buttoncolor = Colorf::fromRgba8(78, 78, 78);
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, h), buttoncolor);
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, 1.0), bordercolor);
	canvas->fillRect(Rect::xywh(0.0, h - 1.0, w, 1.0), bordercolor);
	canvas->fillRect(Rect::xywh(0.0, 0.0, 1.0, h - 0.0), bordercolor);
	canvas->fillRect(Rect::xywh(w - 1.0, 0.0, 1.0, h - 0.0), bordercolor);
}

void PushButton::OnPaint(Canvas* canvas)
{
	Rect box = canvas->measureText(text);
	canvas->drawText(Point((GetWidth() - box.width) * 0.5, GetHeight() - 5.0), Colorf::fromRgba8(255, 255, 255), text);
}

void PushButton::OnMouseMove(const Point& pos)
{
	if (!hot)
	{
		hot = true;
		Update();
	}
}

bool PushButton::OnMouseDown(const Point& pos, int key)
{
	SetFocus();
	buttonDown = true;
	Update();
	return true;
}

bool PushButton::OnMouseUp(const Point& pos, int key)
{
	if (buttonDown)
	{
		buttonDown = false;
		hot = false;
		Repaint();
		Click();
	}
	return true;
}

void PushButton::OnMouseLeave()
{
	hot = false;
	buttonDown = false;
	Update();
}

void PushButton::OnKeyDown(EInputKey key)
{
	if (key == IK_Space || key == IK_Enter)
	{
		buttonDown = true;
		Update();
	}
}

void PushButton::OnKeyUp(EInputKey key)
{
	if (key == IK_Space || key == IK_Enter)
	{
		buttonDown = false;
		hot = false;
		Repaint();
		Click();
	}
}

void PushButton::Click()
{
	if (OnClick)
		OnClick();
}
