
#include "widgets/mainwindow/mainwindow.h"
#include "widgets/menubar/menubar.h"
#include "widgets/toolbar/toolbar.h"
#include "widgets/statusbar/statusbar.h"

MainWindow::MainWindow() : Widget(nullptr, WidgetType::Window)
{
	MenubarWidget = new Menubar(this);
	// ToolbarWidget = new Toolbar(this);
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
	// ToolbarWidget->SetFrameGeometry(0.0, 32.0, s.width, 36.0);
	StatusbarWidget->SetFrameGeometry(0.0, s.height - 32.0, s.width, 32.0);

	if (CentralWidget)
		CentralWidget->SetFrameGeometry(0.0, 32.0, s.width, s.height - 32.0 - 32.0);
}
