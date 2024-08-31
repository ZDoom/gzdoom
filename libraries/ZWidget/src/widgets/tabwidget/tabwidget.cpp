
#include "widgets/tabwidget/tabwidget.h"
#include "widgets/textlabel/textlabel.h"
#include "widgets/imagebox/imagebox.h"
#include <algorithm>

TabWidget::TabWidget(Widget* parent) : Widget(parent)
{
	Bar = new TabBar(this);
	PageStack = new TabWidgetStack(this);

	Bar->OnCurrentChanged = [=]() { OnBarCurrentChanged(); };
}

int TabWidget::AddTab(Widget* page, const std::string& label)
{
	return AddTab(page, nullptr, label);
}

int TabWidget::AddTab(Widget* page, const std::shared_ptr<Image>& icon, const std::string& label)
{
	int pageIndex = Bar->AddTab(label);
	page->SetParent(PageStack);
	page->SetVisible(false);
	Pages.push_back(page);
	if (Pages.size() == 1)
	{
		PageStack->SetCurrentWidget(page);
	}
	return pageIndex;
}

void TabWidget::SetTabText(int index, const std::string& text)
{
	Bar->SetTabText(index, text);
}

void TabWidget::SetTabText(Widget* page, const std::string& text)
{
	int index = GetPageIndex(page);
	if (index != -1)
		SetTabText(index, text);
}

void TabWidget::SetTabIcon(int index, const std::shared_ptr<Image>& icon)
{
	Bar->SetTabIcon(index, icon);
}

void TabWidget::SetTabIcon(Widget* page, const std::shared_ptr<Image>& icon)
{
	int index = GetPageIndex(page);
	if (index != -1)
		SetTabIcon(index, icon);
}

int TabWidget::GetCurrentIndex() const
{
	return Bar->GetCurrentIndex();
}

Widget* TabWidget::GetCurrentWidget() const
{
	return Pages[Bar->GetCurrentIndex()];
}

void TabWidget::SetCurrentIndex(int pageIndex)
{
	if (Bar->GetCurrentIndex() != pageIndex)
	{
		Bar->SetCurrentIndex(pageIndex);
		PageStack->SetCurrentWidget(Pages[pageIndex]);
	}
}

void TabWidget::SetCurrentWidget(Widget* pageWidget)
{
	int pageIndex = GetPageIndex(pageWidget);
	if (pageIndex != -1)
		SetCurrentIndex(pageIndex);
}

int TabWidget::GetPageIndex(Widget* pageWidget) const
{
	for (size_t i = 0; i < Pages.size(); i++)
	{
		if (Pages[i] == pageWidget)
			return i;
	}
	return -1;
}

void TabWidget::OnBarCurrentChanged()
{
	int pageIndex = Bar->GetCurrentIndex();
	PageStack->SetCurrentWidget(Pages[pageIndex]);
	if (OnCurrentChanged)
		OnCurrentChanged();
}

void TabWidget::OnPaintFrame(Canvas* canvas)
{
}

void TabWidget::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();
	double barHeight = Bar->GetPreferredHeight();
	Bar->SetFrameGeometry(Rect::xywh(0.0, 0.0, w, barHeight));
	PageStack->SetFrameGeometry(Rect::xywh(0.0, barHeight, w, std::max(h - barHeight, 0.0)));
}

/////////////////////////////////////////////////////////////////////////////

TabBar::TabBar(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(20.0, 0.0, 20.0, 0.0);
}

int TabBar::AddTab(const std::string& label)
{
	return AddTab(nullptr, label);
}

int TabBar::AddTab(const std::shared_ptr<Image>& icon, const std::string& label)
{
	TabBarTab* tab = new TabBarTab(this);
	tab->SetIcon(icon);
	tab->SetText(label);
	tab->OnClick = [=]() { OnTabClicked(tab); };
	int pageIndex = Tabs.size();
	Tabs.push_back(tab);
	if (CurrentIndex == -1)
		SetCurrentIndex(pageIndex);
	OnGeometryChanged();
	return pageIndex;
}

void TabBar::SetTabText(int index, const std::string& text)
{
	Tabs[index]->SetText(text);
	OnGeometryChanged();
}

void TabBar::SetTabIcon(int index, const std::shared_ptr<Image>& icon)
{
	Tabs[index]->SetIcon(icon);
	OnGeometryChanged();
}

int TabBar::GetCurrentIndex() const
{
	return CurrentIndex;
}

void TabBar::SetCurrentIndex(int pageIndex)
{
	if (CurrentIndex != pageIndex)
	{
		if (CurrentIndex != -1)
			Tabs[CurrentIndex]->SetCurrent(false);
		CurrentIndex = pageIndex;
		if (CurrentIndex != -1)
			Tabs[CurrentIndex]->SetCurrent(true);
	}
}

void TabBar::OnTabClicked(TabBarTab* tab)
{
	int pageIndex = GetTabIndex(tab);
	if (CurrentIndex != pageIndex)
	{
		SetCurrentIndex(pageIndex);
		if (OnCurrentChanged)
			OnCurrentChanged();
	}
}

int TabBar::GetTabIndex(TabBarTab* tab)
{
	for (size_t i = 0; i < Tabs.size(); i++)
	{
		if (Tabs[i] == tab)
			return i;
	}
	return -1;
}

void TabBar::OnPaintFrame(Canvas* canvas)
{
	double w = GetFrameGeometry().width;
	double h = GetFrameGeometry().height;
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, h), Colorf::fromRgba8(38, 38, 38));
}

void TabBar::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();
	double x = 0.0;
	for (TabBarTab* tab : Tabs)
	{
		double tabWidth = tab->GetNoncontentLeft() + tab->GetPreferredWidth() + tab->GetNoncontentRight();
		tab->SetFrameGeometry(Rect::xywh(x, 0.0, tabWidth, h));
		x += tabWidth;
	}
}

/////////////////////////////////////////////////////////////////////////////

TabBarTab::TabBarTab(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(15.0, 0.0, 15.0, 0.0);
}

void TabBarTab::SetText(const std::string& text)
{
	if (!text.empty())
	{
		if (!Label)
		{
			Label = new TextLabel(this);
			OnGeometryChanged();
		}
		Label->SetText(text);
	}
	else
	{
		delete Label;
		Label = nullptr;
		OnGeometryChanged();
	}
}

void TabBarTab::SetIcon(const std::shared_ptr<Image>& image)
{
	if (image)
	{
		if (!Icon)
		{
			Icon = new ImageBox(this);
			OnGeometryChanged();
		}
		Icon->SetImage(image);
	}
	else
	{
		delete Icon;
		Icon = nullptr;
		OnGeometryChanged();
	}
}

void TabBarTab::SetCurrent(bool value)
{
	if (IsCurrent != value)
	{
		IsCurrent = value;
		Update();
	}
}

double TabBarTab::GetPreferredWidth() const
{
	double x = Icon ? 32.0 + 5.0 : 0.0;
	if (Label) x += Label->GetPreferredWidth();
	return x;
}

void TabBarTab::OnPaintFrame(Canvas* canvas)
{
	double w = GetFrameGeometry().width;
	double h = GetFrameGeometry().height;
	if (IsCurrent)
	{
		canvas->fillRect(Rect::xywh(0.0, 0.0, w, h), Colorf::fromRgba8(51, 51, 51));
	}
	else if (hot)
	{
		canvas->fillRect(Rect::xywh(0.0, 0.0, w, h), Colorf::fromRgba8(45, 45, 45));
	}
}

void TabBarTab::OnGeometryChanged()
{
	double x = 0.0;
	double w = GetWidth();
	double h = GetHeight();
	if (Icon)
	{
		Icon->SetFrameGeometry(Rect::xywh(x, (h - 32.0) * 0.5, 32.0, 32.0));
		x = 32.0 + 5.0;
	}
	if (Label)
	{
		Label->SetFrameGeometry(Rect::xywh(x, (h - Label->GetPreferredHeight()) * 0.5, std::max(w - x, 0.0), Label->GetPreferredHeight()));
	}
}

void TabBarTab::OnMouseMove(const Point& pos)
{
	if (!hot)
	{
		hot = true;
		Update();
	}
}

bool TabBarTab::OnMouseDown(const Point& pos, int key)
{
	if (OnClick)
		OnClick();
	return true;
}

bool TabBarTab::OnMouseUp(const Point& pos, int key)
{
	return true;
}

void TabBarTab::OnMouseLeave()
{
	hot = false;
	Update();
}

/////////////////////////////////////////////////////////////////////////////

TabWidgetStack::TabWidgetStack(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(20.0, 5.0, 20.0, 5.0);
}

void TabWidgetStack::SetCurrentWidget(Widget* widget)
{
	if (widget != CurrentWidget)
	{
		if (CurrentWidget)
			CurrentWidget->SetVisible(false);
		CurrentWidget = widget;
		if (CurrentWidget)
		{
			CurrentWidget->SetVisible(true);
			OnGeometryChanged();
		}
	}
}

void TabWidgetStack::OnPaintFrame(Canvas* canvas)
{
}

void TabWidgetStack::OnGeometryChanged()
{
	if (CurrentWidget)
		CurrentWidget->SetFrameGeometry(Rect::xywh(0.0, 0.0, GetWidth(), GetHeight()));
}
