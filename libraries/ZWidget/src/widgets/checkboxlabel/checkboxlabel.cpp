
#include "widgets/checkboxlabel/checkboxlabel.h"

CheckboxLabel::CheckboxLabel(Widget* parent) : Widget(parent)
{
	SetStyleClass("checkbox-label");
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
	// To do: add and use GetStyleImage for the checkbox
	
	double center = GridFitPoint(GetHeight() * 0.5);
	double borderwidth = GridFitSize(1.0);
	double outerboxsize = GridFitSize(10.0);
	double innerboxsize = outerboxsize - 2.0 * borderwidth;
	double checkedsize = innerboxsize - 2.0 * borderwidth;

	if (checked)
	{
		canvas->fillRect(Rect::xywh(0.0, center - 6.0 * borderwidth, outerboxsize, outerboxsize), GetStyleColor("checked-outer-border-color"));
		canvas->fillRect(Rect::xywh(1.0 * borderwidth, center - 5.0 * borderwidth, innerboxsize, innerboxsize), GetStyleColor("checked-inner-border-color"));
		canvas->fillRect(Rect::xywh(2.0 * borderwidth, center - 4.0 * borderwidth, checkedsize, checkedsize), GetStyleColor("checked-color"));
	}
	else
	{
		canvas->fillRect(Rect::xywh(0.0, center - 6.0 * borderwidth, outerboxsize, outerboxsize), GetStyleColor("unchecked-outer-border-color"));
		canvas->fillRect(Rect::xywh(1.0 * borderwidth, center - 5.0 * borderwidth, innerboxsize, innerboxsize), GetStyleColor("unchecked-inner-border-color"));
	}

	canvas->drawText(Point(14.0, GetHeight() - 5.0), GetStyleColor("color"), text);
}

bool CheckboxLabel::OnMouseDown(const Point& pos, InputKey key)
{
	mouseDownActive = true;
	SetFocus();
	return true;
}

bool CheckboxLabel::OnMouseUp(const Point& pos, InputKey key)
{
	if (mouseDownActive)
	{
		Toggle();
	}
	mouseDownActive = false;
	return true;
}

void CheckboxLabel::OnMouseLeave()
{
	mouseDownActive = false;
}

void CheckboxLabel::OnKeyUp(InputKey key)
{
	if (key == InputKey::Space)
		Toggle();
}

void CheckboxLabel::Toggle()
{
	bool oldchecked = checked;
	checked = radiostyle? true : !checked;
	Update();
	if (checked != oldchecked && FuncChanged) FuncChanged(checked);
}
