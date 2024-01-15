
#pragma once

#include "../../core/widget.h"

class Menubar;
class Toolbar;
class Statusbar;

class MainWindow : public Widget
{
public:
	MainWindow();
	~MainWindow();

	Menubar* GetMenubar() const { return MenubarWidget; }
	Toolbar* GetToolbar() const { return ToolbarWidget; }
	Statusbar* GetStatusbar() const { return StatusbarWidget; }
	Widget* GetCentralWidget() const { return CentralWidget; }

	void SetCentralWidget(Widget* widget);

protected:
	void OnGeometryChanged() override;

private:
	Menubar* MenubarWidget = nullptr;
	Toolbar* ToolbarWidget = nullptr;
	Widget* CentralWidget = nullptr;
	Statusbar* StatusbarWidget = nullptr;
};
