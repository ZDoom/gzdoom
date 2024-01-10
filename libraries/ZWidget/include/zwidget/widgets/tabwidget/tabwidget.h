
#pragma once

#include "../../core/widget.h"
#include <functional>
#include <string>

class TabBar;
class TabBarTab;
class TabWidgetStack;
class TextLabel;
class ImageBox;
class Image;

class TabWidget : public Widget
{
public:
	TabWidget(Widget* parent);

	int AddTab(Widget* page, const std::string& label);
	int AddTab(Widget* page, const std::shared_ptr<Image>& icon, const std::string& label);

	int GetCurrentIndex() const;
	Widget* GetCurrentWidget() const;

	int GetPageIndex(Widget* pageWidget) const;

	void SetCurrentIndex(int pageIndex);
	void SetCurrentWidget(Widget* pageWidget);

	std::function<void()> OnCurrentChanged;

protected:
	void OnPaintFrame(Canvas* canvas) override;
	void OnGeometryChanged() override;

private:
	void OnBarCurrentChanged();

	TabBar* Bar = nullptr;
	TabWidgetStack* PageStack = nullptr;
	std::vector<Widget*> Pages;
};

class TabBar : public Widget
{
public:
	TabBar(Widget* parent);

	int AddTab(const std::string& label);
	int AddTab(const std::shared_ptr<Image>& icon, const std::string& label);

	int GetCurrentIndex() const;
	void SetCurrentIndex(int pageIndex);

	double GetPreferredHeight() const { return 30.0; }

	std::function<void()> OnCurrentChanged;

protected:
	void OnPaintFrame(Canvas* canvas) override;
	void OnGeometryChanged() override;

private:
	void OnTabClicked(TabBarTab* tab);
	int GetTabIndex(TabBarTab* tab);

	int CurrentIndex = -1;
	std::vector<TabBarTab*> Tabs;
};

class TabBarTab : public Widget
{
public:
	TabBarTab(Widget* parent);

	void SetText(const std::string& text);
	void SetIcon(const std::shared_ptr<Image>& icon);
	void SetCurrent(bool value);

	double GetPreferredWidth() const;

	std::function<void()> OnClick;

protected:
	void OnPaintFrame(Canvas* canvas) override;
	void OnGeometryChanged() override;
	void OnMouseMove(const Point& pos) override;
	void OnMouseDown(const Point& pos, int key) override;
	void OnMouseUp(const Point& pos, int key) override;
	void OnMouseLeave() override;

private:
	bool IsCurrent = false;

	ImageBox* Icon = nullptr;
	TextLabel* Label = nullptr;
	bool mouseDown = false;
	bool hot = false;
};

class TabWidgetStack : public Widget
{
public:
	TabWidgetStack(Widget* parent);

	void SetCurrentWidget(Widget* widget);
	Widget* GetCurrentWidget() const { return CurrentWidget; }

protected:
	void OnPaintFrame(Canvas* canvas) override;
	void OnGeometryChanged() override;

private:
	Widget* CurrentWidget = nullptr;
};
