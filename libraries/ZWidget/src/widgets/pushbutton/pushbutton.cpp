
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
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, h), Colorf::fromRgba8(68, 68, 68));
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
