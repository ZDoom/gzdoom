
#include "widgets/mainwindow/mainwindow.h"
#include "widgets/menubar/menubar.h"
#include "widgets/toolbar/toolbar.h"
#include "widgets/statusbar/statusbar.h"

MainWindow::MainWindow(RenderAPI api) : Widget(nullptr, WidgetType::Window, api)
{
	MenubarWidget = new Menubar(this);
	TopToolbarWidget = new Toolbar(this);
	TopToolbarWidget->SetDirection(ToolbarDirection::Horizontal);
	LeftToolbarWidget = new Toolbar(this);
	LeftToolbarWidget->SetDirection(ToolbarDirection::Vertical);
	StatusbarWidget = new Statusbar(this);
}

MainWindow::~MainWindow()
{
}

void MainWindow::SetCentralWidget(Widget* widget)
{
	if (CentralWidget != widget)
	{
		delete CentralWidget;
		CentralWidget = widget;
		if (CentralWidget)
			CentralWidget->SetParent(this);
		OnGeometryChanged();
	}
}

void MainWindow::OnGeometryChanged()
{
	Size s = GetSize();

	MenubarWidget->SetFrameGeometry(0.0, 0.0, s.width, 32.0);
	TopToolbarWidget->SetFrameGeometry(0.0, 32.0, s.width, 32.0);
	LeftToolbarWidget->SetFrameGeometry(0.0, 64.0, 32.0, s.height - 64.0);
	StatusbarWidget->SetFrameGeometry(0.0, s.height - 32.0, s.width, 32.0);

	if (CentralWidget)
		CentralWidget->SetFrameGeometry(32.0, 64.0, s.width - 32.0, s.height - 64.0 - 32.0);
}
