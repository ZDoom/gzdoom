
#pragma once

#include "../../core/widget.h"

class Menubar;
class Toolbar;
class Statusbar;

class MainWindow : public Widget
{
public:
	MainWindow(RenderAPI api = RenderAPI::Unspecified);
	~MainWindow();

	Menubar* GetMenubar() const { return MenubarWidget; }
	Toolbar* GetTopToolbar() const { return TopToolbarWidget; }
	Toolbar* GetLeftToolbar() const { return LeftToolbarWidget; }
	Statusbar* GetStatusbar() const { return StatusbarWidget; }
	Widget* GetCentralWidget() const { return CentralWidget; }

	void SetCentralWidget(Widget* widget);

protected:
	void OnGeometryChanged() override;

private:
	Menubar* MenubarWidget = nullptr;
	Toolbar* TopToolbarWidget = nullptr;
	Toolbar* LeftToolbarWidget = nullptr;
	Widget* CentralWidget = nullptr;
	Statusbar* StatusbarWidget = nullptr;
};
