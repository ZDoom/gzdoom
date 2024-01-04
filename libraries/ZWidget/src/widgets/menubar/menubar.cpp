
#include "widgets/menubar/menubar.h"
#include "core/colorf.h"

Menubar::Menubar(Widget* parent) : Widget(parent)
{
}

Menubar::~Menubar()
{
}

void Menubar::OnPaint(Canvas* canvas)
{
	canvas->drawText(Point(16.0, 21.0), Colorf::fromRgba8(0, 0, 0), "File      Edit      View      Tools      Window     Help");
}
