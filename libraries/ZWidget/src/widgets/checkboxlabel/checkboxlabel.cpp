
#include "widgets/checkboxlabel/checkboxlabel.h"

CheckboxLabel::CheckboxLabel(Widget* parent) : Widget(parent)
{
}

void CheckboxLabel::SetText(const std::string& value)
{
	if (text != value)
	{
		text = value;
		Update();
	}
}

const std::string& CheckboxLabel::GetText() const
{
	return text;
}

void CheckboxLabel::SetChecked(bool value)
{
	if (value != checked)
	{
		checked = value;
		Update();
	}
}

bool CheckboxLabel::GetChecked() const
{
	return checked;
}

double CheckboxLabel::GetPreferredHeight() const
{
	return 20.0;
}

void CheckboxLabel::OnPaint(Canvas* canvas)
{
	if (checked)
	{
		canvas->fillRect(Rect::xywh(0.0, GetHeight() * 0.5 - 5.0, 10.0, 10.0), Colorf::fromRgba8(100, 100, 100));
		canvas->fillRect(Rect::xywh(1.0, GetHeight() * 0.5 - 4.0, 8.0, 8.0), Colorf::fromRgba8(51, 51, 51));
		canvas->fillRect(Rect::xywh(2.0, GetHeight() * 0.5 - 3.0, 6.0, 6.0), Colorf::fromRgba8(226, 223, 219));
	}
	else
	{
		canvas->fillRect(Rect::xywh(0.0, GetHeight() * 0.5 - 5.0, 10.0, 10.0), Colorf::fromRgba8(68, 68, 68));
		canvas->fillRect(Rect::xywh(1.0, GetHeight() * 0.5 - 4.0, 8.0, 8.0), Colorf::fromRgba8(51, 51, 51));
	}

	canvas->drawText(Point(14.0, GetHeight() - 5.0), Colorf::fromRgba8(255, 255, 255), text);
}

void CheckboxLabel::OnMouseDown(const Point& pos, int key)
{
	mouseDownActive = true;
	SetFocus();
}

void CheckboxLabel::OnMouseUp(const Point& pos, int key)
{
	if (mouseDownActive)
	{
		Toggle();
	}
	mouseDownActive = false;
}

void CheckboxLabel::OnMouseLeave()
{
	mouseDownActive = false;
}

void CheckboxLabel::OnKeyUp(EInputKey key)
{
	if (key == IK_Space)
		Toggle();
}

void CheckboxLabel::Toggle()
{
	checked = !checked;
	Update();
}
