
#include "widgets/textlabel/textlabel.h"

TextLabel::TextLabel(Widget* parent) : Widget(parent)
{
}

void TextLabel::SetText(const std::string& value)
{
	if (text != value)
	{
		text = value;
		Update();
	}
}

const std::string& TextLabel::GetText() const
{
	return text;
}

double TextLabel::GetPreferredHeight() const
{
	return 20.0;
}

void TextLabel::OnPaint(Canvas* canvas)
{
	canvas->drawText(Point(0.0, GetHeight() - 5.0), Colorf::fromRgba8(255, 255, 255), text);
}
