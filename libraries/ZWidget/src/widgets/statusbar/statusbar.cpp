
#include "widgets/statusbar/statusbar.h"
#include "widgets/lineedit/lineedit.h"
#include "core/colorf.h"

Statusbar::Statusbar(Widget* parent) : Widget(parent)
{
	CommandEdit = new LineEdit(this);
	CommandEdit->SetFrameGeometry(Rect::xywh(90.0, 4.0, 400.0, 23.0));
}

Statusbar::~Statusbar()
{
}

void Statusbar::OnPaint(Canvas* canvas)
{
	canvas->drawText(Point(16.0, 21.0), Colorf::fromRgba8(0, 0, 0), "Command:");
}
