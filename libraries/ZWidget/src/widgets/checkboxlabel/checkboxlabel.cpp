
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
		canvas->drawText(Point(0.0, GetHeight() - 5.0), Colorf::fromRgba8(255, 255, 255), "[x] " + text);
	else
		canvas->drawText(Point(0.0, GetHeight() - 5.0), Colorf::fromRgba8(255, 255, 255), "[ ] " + text);
}
