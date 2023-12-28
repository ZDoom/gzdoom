
#include "errorwindow.h"
#include "version.h"
#include <zwidget/widgets/textedit/textedit.h>

void ErrorWindow::ExecModal(const std::string& text)
{
	Size screenSize = GetScreenSize();
	double windowWidth = 1200.0;
	double windowHeight = 700.0;

	auto window = new ErrorWindow();
	window->SetText(text);
	window->SetFrameGeometry((screenSize.width - windowWidth) * 0.5, (screenSize.height - windowHeight) * 0.5, windowWidth, windowHeight);
	window->Show();

	DisplayWindow::RunLoop();
}

ErrorWindow::ErrorWindow() : Widget(nullptr, WidgetType::Window)
{
	FStringf caption("Fatal Error - " GAMENAME " %s (%s)", GetVersionString(), GetGitTime());
	SetWindowTitle(caption.GetChars());
	SetWindowBackground(Colorf::fromRgba8(51, 51, 51));
	SetWindowBorderColor(Colorf::fromRgba8(51, 51, 51));
	SetWindowCaptionColor(Colorf::fromRgba8(33, 33, 33));
	SetWindowCaptionTextColor(Colorf::fromRgba8(226, 223, 219));

	LogView = new TextEdit(this);

	LogView->SetFocus();
}

void ErrorWindow::SetText(const std::string& text)
{
	LogView->SetText(text);
}

void ErrorWindow::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();
	LogView->SetFrameGeometry(Rect::xywh(20.0, 20.0, w - 40.0, h - 40.0));
}
