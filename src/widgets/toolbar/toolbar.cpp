
#include "widgets/toolbar/toolbar.h"
#include "widgets/toolbar/toolbarbutton.h"

Toolbar::Toolbar(Widget* parent) : Widget(parent)
{
	SetStyleClass("toolbar");
}

Toolbar::~Toolbar()
{
}

void Toolbar::SetDirection(ToolbarDirection newDirection)
{
	if (direction != newDirection)
	{
		direction = newDirection;
		Update();
	}
}

ToolbarButton* Toolbar::AddButton(std::string icon, std::string text, std::function<void()> onClicked)
{
	ToolbarButton* button = new ToolbarButton(this);
	if (!icon.empty())
		button->SetIcon(icon);
	if (!text.empty())
		button->SetText(text);
	button->OnClick = std::move(onClicked);
	buttons.push_back(button);
	return button;
}

void Toolbar::OnGeometryChanged()
{
	if (direction == ToolbarDirection::Horizontal)
	{
		double x = 7.0;
		double barHeight = GetHeight();
		double gap = 7.0;
		for (ToolbarButton* button : buttons)
		{
			double width = button->GetPreferredWidth();
			double height = button->GetPreferredHeight();
			button->SetFrameGeometry(Rect::xywh(x, (barHeight - height) * 0.5, width, height));
			x += width + gap;
		}
	}
	else
	{
		double y = 7.0;
		double barWidth = GetWidth();
		double gap = 7.0;
		for (ToolbarButton* button : buttons)
		{
			double width = button->GetPreferredWidth();
			double height = button->GetPreferredHeight();
			button->SetFrameGeometry(Rect::xywh((barWidth - width) * 0.5, y, width, height));
			y += height + gap;
		}
	}
}
