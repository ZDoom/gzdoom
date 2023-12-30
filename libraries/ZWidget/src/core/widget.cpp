
#include "core/widget.h"
#include "core/timer.h"
#include "core/colorf.h"
#include <stdexcept>

Widget::Widget(Widget* parent, WidgetType type) : Type(type)
{
	if (type != WidgetType::Child)
	{
		DispWindow = DisplayWindow::Create(this);
		DispCanvas = Canvas::create(DispWindow.get());
	}

	SetParent(parent);
}

Widget::~Widget()
{
	while (LastChildObj)
		delete LastChildObj;

	while (FirstTimerObj)
		delete FirstTimerObj;

	DetachFromParent();
}

void Widget::SetParent(Widget* newParent)
{
	if (ParentObj != newParent)
	{
		if (ParentObj)
			DetachFromParent();

		if (newParent)
		{
			PrevSiblingObj = newParent->LastChildObj;
			if (PrevSiblingObj) PrevSiblingObj->NextSiblingObj = this;
			newParent->LastChildObj = this;
			if (!newParent->FirstChildObj) newParent->FirstChildObj = this;
			ParentObj = newParent;
		}
	}
}

void Widget::MoveBefore(Widget* sibling)
{
	if (sibling && sibling->ParentObj != ParentObj) throw std::runtime_error("Invalid sibling passed to Widget.MoveBefore");
	if (!ParentObj) throw std::runtime_error("Widget must have a parent before it can be moved");

	if (NextSiblingObj != sibling)
	{
		Widget* p = ParentObj;
		DetachFromParent();

		ParentObj = p;
		if (sibling)
		{
			NextSiblingObj = sibling;
			PrevSiblingObj = sibling->PrevSiblingObj;
			sibling->PrevSiblingObj = this;
			if (PrevSiblingObj) PrevSiblingObj->NextSiblingObj = this;
			if (ParentObj->FirstChildObj == sibling) ParentObj->FirstChildObj = this;
		}
		else
		{
			PrevSiblingObj = ParentObj->LastChildObj;
			if (PrevSiblingObj) PrevSiblingObj->NextSiblingObj = this;
			ParentObj->LastChildObj = this;
			if (!ParentObj->FirstChildObj) ParentObj->FirstChildObj = this;
		}
	}
}

void Widget::DetachFromParent()
{
	for (Widget* cur = ParentObj; cur; cur = cur->ParentObj)
	{
		if (cur->FocusWidget == this)
			cur->FocusWidget = nullptr;
		if (cur->CaptureWidget == this)
			cur->CaptureWidget = nullptr;
		if (cur->HoverWidget == this)
			cur->HoverWidget = nullptr;

		if (cur->DispWindow)
			break;
	}

	if (PrevSiblingObj)
		PrevSiblingObj->NextSiblingObj = NextSiblingObj;
	if (NextSiblingObj)
		NextSiblingObj->PrevSiblingObj = PrevSiblingObj;
	if (ParentObj)
	{
		if (ParentObj->FirstChildObj == this)
			ParentObj->FirstChildObj = NextSiblingObj;
		if (ParentObj->LastChildObj == this)
			ParentObj->LastChildObj = PrevSiblingObj;
	}
	PrevSiblingObj = nullptr;
	NextSiblingObj = nullptr;
	ParentObj = nullptr;
}

std::string Widget::GetWindowTitle() const
{
	return WindowTitle;
}

void Widget::SetWindowTitle(const std::string& text)
{
	if (WindowTitle != text)
	{
		WindowTitle = text;
		if (DispWindow)
			DispWindow->SetWindowTitle(WindowTitle);
	}
}

Size Widget::GetSize() const
{
	return ContentGeometry.size();
}

Rect Widget::GetFrameGeometry() const
{
	if (Type == WidgetType::Child)
	{
		return FrameGeometry;
	}
	else
	{
		return DispWindow->GetWindowFrame();
	}
}

void Widget::SetNoncontentSizes(double left, double top, double right, double bottom)
{
	Noncontent.Left = left;
	Noncontent.Top = top;
	Noncontent.Right = right;
	Noncontent.Bottom = bottom;
}

void Widget::SetFrameGeometry(const Rect& geometry)
{
	if (Type == WidgetType::Child)
	{
		FrameGeometry = geometry;
		double left = FrameGeometry.left() + Noncontent.Left;
		double top = FrameGeometry.top() + Noncontent.Top;
		double right = FrameGeometry.right() - Noncontent.Right;
		double bottom = FrameGeometry.bottom() - Noncontent.Bottom;
		left = std::min(left, FrameGeometry.right());
		top = std::min(top, FrameGeometry.bottom());
		right = std::max(right, FrameGeometry.left());
		bottom = std::max(bottom, FrameGeometry.top());
		ContentGeometry = Rect::ltrb(left, top, right, bottom);
		OnGeometryChanged();
	}
	else
	{
		DispWindow->SetWindowFrame(geometry);
	}
}

void Widget::Show()
{
	if (Type != WidgetType::Child)
	{
		DispWindow->Show();
	}
}

void Widget::ShowFullscreen()
{
	if (Type != WidgetType::Child)
	{
		DispWindow->ShowFullscreen();
	}
}

void Widget::ShowMaximized()
{
	if (Type != WidgetType::Child)
	{
		DispWindow->ShowMaximized();
	}
}

void Widget::ShowMinimized()
{
	if (Type != WidgetType::Child)
	{
		DispWindow->ShowMinimized();
	}
}

void Widget::ShowNormal()
{
	if (Type != WidgetType::Child)
	{
		DispWindow->ShowNormal();
	}
}

void Widget::Hide()
{
	if (Type != WidgetType::Child)
	{
		if (DispWindow)
			DispWindow->Hide();
	}
}

void Widget::ActivateWindow()
{
	if (Type != WidgetType::Child)
	{
		DispWindow->Activate();
	}
}

void Widget::Close()
{
	OnClose();
}

void Widget::SetWindowBackground(const Colorf& color)
{
	Widget* w = Window();
	if (w && w->WindowBackground != color)
	{
		w->WindowBackground = color;
		Update();
	}
}

void Widget::SetWindowBorderColor(const Colorf& color)
{
	Widget* w = Window();
	if (w)
	{
		w->DispWindow->SetBorderColor(color.toBgra8());
		w->DispWindow->Update();
	}
}

void Widget::SetWindowCaptionColor(const Colorf& color)
{
	Widget* w = Window();
	if (w)
	{
		w->DispWindow->SetCaptionColor(color.toBgra8());
		w->DispWindow->Update();
	}
}

void Widget::SetWindowCaptionTextColor(const Colorf& color)
{
	Widget* w = Window();
	if (w)
	{
		w->DispWindow->SetCaptionTextColor(color.toBgra8());
		w->DispWindow->Update();
	}
}

void Widget::Update()
{
	Widget* w = Window();
	if (w)
	{
		w->DispWindow->Update();
	}
}

void Widget::Repaint()
{
	Widget* w = Window();
	w->DispCanvas->begin(WindowBackground);
	w->Paint(w->DispCanvas.get());
	w->DispCanvas->end();
}

void Widget::Paint(Canvas* canvas)
{
	Point oldOrigin = canvas->getOrigin();
	canvas->pushClip(FrameGeometry);
	canvas->setOrigin(oldOrigin + FrameGeometry.topLeft());
	OnPaintFrame(canvas);
	canvas->setOrigin(oldOrigin);
	canvas->popClip();

	canvas->pushClip(ContentGeometry);
	canvas->setOrigin(oldOrigin + ContentGeometry.topLeft());
	OnPaint(canvas);
	for (Widget* w = FirstChild(); w != nullptr; w = w->NextSibling())
	{
		if (w->Type == WidgetType::Child)
			w->Paint(canvas);
	}
	canvas->setOrigin(oldOrigin);
	canvas->popClip();
}

bool Widget::GetKeyState(EInputKey key)
{
	Widget* window = Window();
	return window ? window->DispWindow->GetKeyState(key) : false;
}

bool Widget::HasFocus()
{
	Widget* window = Window();
	return window ? window->FocusWidget == this : false;
}

bool Widget::IsEnabled()
{
	return true;
}

bool Widget::IsVisible()
{
	return true;
}

void Widget::SetFocus()
{
	Widget* window = Window();
	if (window)
	{
		if (window->FocusWidget)
			window->FocusWidget->OnLostFocus();
		window->FocusWidget = this;
		window->FocusWidget->OnSetFocus();
		window->ActivateWindow();
	}
}

void Widget::SetEnabled(bool value)
{
}

void Widget::LockCursor()
{
	Widget* w = Window();
	if (w && w->CaptureWidget != this)
	{
		w->CaptureWidget = this;
		w->DispWindow->LockCursor();
	}
}

void Widget::UnlockCursor()
{
	Widget* w = Window();
	if (w && w->CaptureWidget != nullptr)
	{
		w->CaptureWidget = nullptr;
		w->DispWindow->UnlockCursor();
	}
}

void Widget::SetCursor(StandardCursor cursor)
{
}

void Widget::CaptureMouse()
{
}

void Widget::ReleaseMouseCapture()
{
}

std::string Widget::GetClipboardText()
{
	Widget* w = Window();
	if (w)
		return w->DispWindow->GetClipboardText();
	else
		return {};
}

void Widget::SetClipboardText(const std::string& text)
{
	Widget* w = Window();
	if (w)
		w->DispWindow->SetClipboardText(text);
}

Widget* Widget::Window()
{
	for (Widget* w = this; w != nullptr; w = w->Parent())
	{
		if (w->DispWindow)
			return w;
	}
	return nullptr;
}

Canvas* Widget::GetCanvas()
{
	for (Widget* w = this; w != nullptr; w = w->Parent())
	{
		if (w->DispCanvas)
			return w->DispCanvas.get();
	}
	return nullptr;
}

Widget* Widget::ChildAt(const Point& pos)
{
	for (Widget* cur = LastChild(); cur != nullptr; cur = cur->PrevSibling())
	{
		if (cur->FrameGeometry.contains(pos))
		{
			Widget* cur2 = cur->ChildAt(pos - cur->FrameGeometry.topLeft());
			return cur2 ? cur2 : cur;
		}
	}
	return nullptr;
}

Point Widget::MapFrom(const Widget* parent, const Point& pos) const
{
	Point p = pos;
	for (const Widget* cur = this; cur != nullptr; cur = cur->Parent())
	{
		if (cur == parent)
			return p;
		p -= cur->ContentGeometry.topLeft();
	}
	throw std::runtime_error("MapFrom: not a parent of widget");
}

Point Widget::MapFromGlobal(const Point& pos) const
{
	Point p = pos;
	for (const Widget* cur = this; cur != nullptr; cur = cur->Parent())
	{
		if (cur->DispWindow)
		{
			return p - cur->GetFrameGeometry().topLeft();
		}
		p -= cur->ContentGeometry.topLeft();
	}
	throw std::runtime_error("MapFromGlobal: no window widget found");
}

Point Widget::MapTo(const Widget* parent, const Point& pos) const
{
	Point p = pos;
	for (const Widget* cur = this; cur != nullptr; cur = cur->Parent())
	{
		if (cur == parent)
			return p;
		p += cur->ContentGeometry.topLeft();
	}
	throw std::runtime_error("MapTo: not a parent of widget");
}

Point Widget::MapToGlobal(const Point& pos) const
{
	Point p = pos;
	for (const Widget* cur = this; cur != nullptr; cur = cur->Parent())
	{
		if (cur->DispWindow)
		{
			return cur->GetFrameGeometry().topLeft() + p;
		}
		p += cur->ContentGeometry.topLeft();
	}
	throw std::runtime_error("MapFromGlobal: no window widget found");
}

void Widget::OnWindowPaint()
{
	Repaint();
}

void Widget::OnWindowMouseMove(const Point& pos)
{
	if (CaptureWidget)
	{
		CaptureWidget->OnMouseMove(CaptureWidget->MapFrom(this, pos));
	}
	else
	{
		Widget* widget = ChildAt(pos);
		if (!widget)
			widget = this;

		if (HoverWidget != widget)
		{
			if (HoverWidget)
				HoverWidget->OnMouseLeave();
			HoverWidget = widget;
		}

		widget->OnMouseMove(widget->MapFrom(this, pos));
	}
}

void Widget::OnWindowMouseDown(const Point& pos, EInputKey key)
{
	if (CaptureWidget)
	{
		CaptureWidget->OnMouseDown(CaptureWidget->MapFrom(this, pos), key);
	}
	else
	{
		Widget* widget = ChildAt(pos);
		if (!widget)
			widget = this;
		widget->OnMouseDown(widget->MapFrom(this, pos), key);
	}
}

void Widget::OnWindowMouseDoubleclick(const Point& pos, EInputKey key)
{
	if (CaptureWidget)
	{
		CaptureWidget->OnMouseDoubleclick(CaptureWidget->MapFrom(this, pos), key);
	}
	else
	{
		Widget* widget = ChildAt(pos);
		if (!widget)
			widget = this;
		widget->OnMouseDoubleclick(widget->MapFrom(this, pos), key);
	}
}

void Widget::OnWindowMouseUp(const Point& pos, EInputKey key)
{
	if (CaptureWidget)
	{
		CaptureWidget->OnMouseUp(CaptureWidget->MapFrom(this, pos), key);
	}
	else
	{
		Widget* widget = ChildAt(pos);
		if (!widget)
			widget = this;
		widget->OnMouseUp(widget->MapFrom(this, pos), key);
	}
}

void Widget::OnWindowMouseWheel(const Point& pos, EInputKey key)
{
	if (CaptureWidget)
	{
		CaptureWidget->OnMouseWheel(CaptureWidget->MapFrom(this, pos), key);
	}
	else
	{
		Widget* widget = ChildAt(pos);
		if (!widget)
			widget = this;
		widget->OnMouseWheel(widget->MapFrom(this, pos), key);
	}
}

void Widget::OnWindowRawMouseMove(int dx, int dy)
{
	if (CaptureWidget)
	{
		CaptureWidget->OnRawMouseMove(dx, dy);
	}
	else if (FocusWidget)
	{
		FocusWidget->OnRawMouseMove(dx, dy);
	}
}

void Widget::OnWindowKeyChar(std::string chars)
{
	if (FocusWidget)
		FocusWidget->OnKeyChar(chars);
}

void Widget::OnWindowKeyDown(EInputKey key)
{
	if (FocusWidget)
		FocusWidget->OnKeyDown(key);
}

void Widget::OnWindowKeyUp(EInputKey key)
{
	if (FocusWidget)
		FocusWidget->OnKeyUp(key);
}

void Widget::OnWindowGeometryChanged()
{
	Size size = DispWindow->GetClientSize();
	FrameGeometry = Rect::xywh(0.0, 0.0, size.width, size.height);
	ContentGeometry = FrameGeometry;
	OnGeometryChanged();
}

void Widget::OnWindowClose()
{
	Close();
}

void Widget::OnWindowActivated()
{
}

void Widget::OnWindowDeactivated()
{
}

void Widget::OnWindowDpiScaleChanged()
{
}

Size Widget::GetScreenSize()
{
	return DisplayWindow::GetScreenSize();
}
