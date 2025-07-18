
#include "widgets/toolbar/toolbarbutton.h"

ToolbarButton::ToolbarButton(Widget* parent) : Widget(parent)
{
	SetStyleClass("toolbarbutton");
}

ToolbarButton::~ToolbarButton()
{
}

void ToolbarButton::SetIcon(std::string icon)
{
	if (!icon.empty())
	{
		if (!image)
			image = new ImageBox(this);
		image->SetImage(Image::LoadResource(icon, GetDpiScale()));
		image->SetImageMode(ImageBoxMode::Contain);
	}
	else
	{
		delete image;
		image = nullptr;
	}
}

void ToolbarButton::SetText(std::string text)
{
	if (!text.empty())
	{
		if (!label)
			label = new TextLabel(this);
		label->SetText(text);
	}
	else
	{
		delete label;
		label = nullptr;
	}
}

void ToolbarButton::Click()
{
	if (OnClick)
		OnClick();
}

double ToolbarButton::GetPreferredWidth()
{
	double w = 0.0;
	if (image)
		w = 26.0;
	if (label)
		w += label->GetPreferredWidth();
	return w;
}

double ToolbarButton::GetPreferredHeight()
{
	double h = 0.0;
	if (image)
		h = 24.0;
	if (label)
		h = std::max(h, label->GetPreferredHeight());
	return h;
}

void ToolbarButton::OnMouseMove(const Point& pos)
{
	if (GetStyleState().empty())
	{
		SetStyleState("hover");
	}
}

void ToolbarButton::OnMouseLeave()
{
	SetStyleState("");
}

bool ToolbarButton::OnMouseDown(const Point& pos, InputKey key)
{
	SetStyleState("down");
	return true;
}

bool ToolbarButton::OnMouseUp(const Point& pos, InputKey key)
{
	if (GetStyleState() == "down")
	{
		SetStyleState("");
		Repaint();
		Click();
	}
	return true;
}

void ToolbarButton::OnGeometryChanged()
{
	double totalHeight = GetPreferredHeight();
	double x = 0.0;
	if (image)
	{
		image->SetFrameGeometry(Rect::xywh(0.0, (totalHeight - 24.0) * 0.5, 24.0, 24.0));
		x += 26.0;
	}
	if (label)
	{
		double labelHeight = label->GetPreferredHeight();
		label->SetFrameGeometry(Rect::xywh(x, (totalHeight - labelHeight) * 0.5, GetWidth() - x, labelHeight));
	}
}
