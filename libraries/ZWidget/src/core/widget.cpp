
#include "core/widget.h"
#include "core/timer.h"
#include "core/colorf.h"
#include "core/theme.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>

Widget::Widget(Widget* parent, WidgetType type, RenderAPI renderAPI) : Type(type)
{
	if (type != WidgetType::Child)
	{
		Widget* owner = parent ? parent->Window() : nullptr;
		DispWindow = DisplayWindow::Create(this, type == WidgetType::Popup, owner ? owner->DispWindow.get() : nullptr, renderAPI);
		if (renderAPI == RenderAPI::Unspecified || renderAPI == RenderAPI::Bitmap)
		{
			DispCanvas = Canvas::create();
			DispCanvas->attach(DispWindow.get());
		}
		SetStyleState("root");

		SetWindowBackground(GetStyleColor("window-background"));
		if (GetStyleColor("window-border").a > 0.0f)
			SetWindowBorderColor(GetStyleColor("window-border"));
		if (GetStyleColor("window-caption-color").a > 0.0f)
			SetWindowCaptionColor(GetStyleColor("window-caption-color"));
		if (GetStyleColor("window-caption-text-color").a > 0.0f)
			SetWindowCaptionTextColor(GetStyleColor("window-caption-text-color"));
	}

	SetParent(parent);
}

Widget::~Widget()
{
	if (DispCanvas)
		DispCanvas->detach();

	while (LastChildObj)
		delete LastChildObj;

	while (FirstTimerObj)
		delete FirstTimerObj;

	DetachFromParent();
}

void Widget::SetCanvas(std::unique_ptr<Canvas> canvas)
{
	if (DispWindow)
	{
		if (DispCanvas)
			DispCanvas->detach();
		DispCanvas = std::move(canvas);
		DispCanvas->attach(DispWindow.get());
	}
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
	SetStyleDouble("noncontent-left", left);
	SetStyleDouble("noncontent-top", top);
	SetStyleDouble("noncontent-right", right);
	SetStyleDouble("noncontent-bottom", bottom);
}

void Widget::SetFrameGeometry(const Rect& geometry)
{
	if (Type == WidgetType::Child)
	{
		FrameGeometry = geometry;
		double left = FrameGeometry.left() + GetNoncontentLeft();
		double top = FrameGeometry.top() + GetNoncontentTop();
		double right = FrameGeometry.right() - GetNoncontentRight();
		double bottom = FrameGeometry.bottom() - GetNoncontentBottom();
		left = std::min(left, FrameGeometry.right());
		top = std::min(top, FrameGeometry.bottom());
		right = std::max(right, FrameGeometry.left());
		bottom = std::max(bottom, FrameGeometry.top());
		left = GridFitPoint(left);
		top = GridFitPoint(top);
		right = GridFitPoint(right);
		bottom = GridFitPoint(bottom);
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
	else if (HiddenFlag)
	{
		HiddenFlag = false;
		Update();
	}
}

void Widget::ShowFullscreen()
{
	if (Type != WidgetType::Child)
	{
		DispWindow->ShowFullscreen();
	}
}

bool Widget::IsFullscreen()
{
	if (Type != WidgetType::Child)
	{
		return DispWindow->IsWindowFullscreen();
	}
	return false;
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
	else if (!HiddenFlag)
	{
		HiddenFlag = true;
		Update();
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
	if (w->DispCanvas)
	{
		w->DispCanvas->begin(WindowBackground);
		w->Paint(w->DispCanvas.get());
		w->DispCanvas->end();
	}
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
		if (w->Type == WidgetType::Child && !w->HiddenFlag)
			w->Paint(canvas);
	}
	canvas->setOrigin(oldOrigin);
	canvas->popClip();
}

void Widget::OnPaintFrame(Canvas* canvas)
{
	WidgetStyle* style = WidgetTheme::GetTheme()->GetStyle(StyleClass);
	if (style)
	{
		style->Paint(this, canvas, GetFrameGeometry().size());
	}
}

bool Widget::GetKeyState(InputKey key)
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

bool Widget::IsHidden()
{
	return !IsVisible();
}

bool Widget::IsVisible()
{
	if (Type != WidgetType::Child)
	{
		return true; // DispWindow->IsVisible();
	}
	else
	{
		return !HiddenFlag;
	}
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
	if (CurrentCursor != cursor)
	{
		CurrentCursor = cursor;
		if (HoverWidget == this || CaptureWidget == this)
		{
			Widget* w = Window();
			if (w)
			{
				w->DispWindow->SetCursor(CurrentCursor);
			}
		}
	}
}

void Widget::SetPointerCapture()
{
	Widget* w = Window();
	if (w && w->CaptureWidget != this)
	{
		w->CaptureWidget = this;
		w->DispWindow->CaptureMouse();
	}
}

void Widget::ReleasePointerCapture()
{
	Widget* w = Window();
	if (w && w->CaptureWidget != nullptr)
	{
		w->CaptureWidget = nullptr;
		w->DispWindow->ReleaseMouseCapture();
	}
}

void Widget::SetModalCapture()
{
	Widget* w = Window();
	if (w && w->CaptureWidget != this)
	{
		w->CaptureWidget = this;
	}
}

void Widget::ReleaseModalCapture()
{
	Widget* w = Window();
	if (w && w->CaptureWidget != nullptr)
	{
		w->CaptureWidget = nullptr;
	}
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

Widget* Widget::Window() const
{
	for (const Widget* w = this; w != nullptr; w = w->Parent())
	{
		if (w->DispWindow)
			return const_cast<Widget*>(w);
	}
	return nullptr;
}

Canvas* Widget::GetCanvas() const
{
	for (const Widget* w = this; w != nullptr; w = w->Parent())
	{
		if (w->DispCanvas)
			return w->DispCanvas.get();
	}
	return nullptr;
}

bool Widget::IsParent(const Widget* w) const
{
	while (w)
	{
		w = w->Parent();
		if (w == this)
			return true;
	}
	return false;
}

bool Widget::IsChild(const Widget* w) const
{
	if (!w)
		return false;
	return w->IsParent(this);
}

Widget* Widget::ChildAt(const Point& pos)
{
	for (Widget* cur = LastChild(); cur != nullptr; cur = cur->PrevSibling())
	{
		if (cur->Type == WidgetType::Child && !cur->HiddenFlag && cur->FrameGeometry.contains(pos))
		{
			Widget* cur2 = cur->ChildAt(pos - cur->ContentGeometry.topLeft());
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
			return cur->DispWindow->MapFromGlobal(p);
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
			return cur->DispWindow->MapToGlobal(p);
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
		DispWindow->SetCursor(CaptureWidget->CurrentCursor);
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
			{
				for (Widget* w = HoverWidget; w != widget && w != this; w = w->Parent())
				{
					Widget* p = w->Parent();
					if (!w->FrameGeometry.contains(p->MapFrom(this, pos)))
					{
						w->OnMouseLeave();
					}
				}
			}
			HoverWidget = widget;
		}

		DispWindow->SetCursor(widget->CurrentCursor);

		do
		{
			widget->OnMouseMove(widget->MapFrom(this, pos));
			if (widget == this)
				break;
			widget = widget->Parent();
		} while (widget);
	}
}

void Widget::OnWindowMouseLeave()
{
	if (HoverWidget)
	{
		for (Widget* w = HoverWidget; w; w = w->Parent())
		{
			w->OnMouseLeave();
		}
		HoverWidget = nullptr;
	}
}

void Widget::OnWindowMouseDown(const Point& pos, InputKey key)
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
		while (widget)
		{
			bool stopPropagation = widget->OnMouseDown(widget->MapFrom(this, pos), key);
			if (stopPropagation || widget == this)
				break;
			widget = widget->Parent();
		}
	}
}

void Widget::OnWindowMouseDoubleclick(const Point& pos, InputKey key)
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
		while (widget)
		{
			bool stopPropagation = widget->OnMouseDoubleclick(widget->MapFrom(this, pos), key);
			if (stopPropagation || widget == this)
				break;
			widget = widget->Parent();
		}
	}
}

void Widget::OnWindowMouseUp(const Point& pos, InputKey key)
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
		while (widget)
		{
			bool stopPropagation = widget->OnMouseUp(widget->MapFrom(this, pos), key);
			if (stopPropagation || widget == this)
				break;
			widget = widget->Parent();
		}
	}
}

void Widget::OnWindowMouseWheel(const Point& pos, InputKey key)
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
		while (widget)
		{
			bool stopPropagation = widget->OnMouseWheel(widget->MapFrom(this, pos), key);
			if (stopPropagation || widget == this)
				break;
			widget = widget->Parent();
		}
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

void Widget::OnWindowKeyDown(InputKey key)
{
	if (FocusWidget)
		FocusWidget->OnKeyDown(key);
}

void Widget::OnWindowKeyUp(InputKey key)
{
	if (FocusWidget)
		FocusWidget->OnKeyUp(key);
}

void Widget::OnWindowGeometryChanged()
{
	if (!DispWindow)
		return;
	Size size = DispWindow->GetClientSize();
	FrameGeometry = Rect::xywh(0.0, 0.0, size.width, size.height);

	double left = FrameGeometry.left() + GetNoncontentLeft();
	double top = FrameGeometry.top() + GetNoncontentTop();
	double right = FrameGeometry.right() - GetNoncontentRight();
	double bottom = FrameGeometry.bottom() - GetNoncontentBottom();
	left = std::min(left, FrameGeometry.right());
	top = std::min(top, FrameGeometry.bottom());
	right = std::max(right, FrameGeometry.left());
	bottom = std::max(bottom, FrameGeometry.top());
	ContentGeometry = Rect::ltrb(left, top, right, bottom);

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

double Widget::GetDpiScale() const
{
	Widget* w = Window();
	return w ? w->DispWindow->GetDpiScale() : 1.0;
}

double Widget::GridFitPoint(double p) const
{
	double dpiscale = GetDpiScale();
	return std::round(p * dpiscale) / dpiscale;
}

double Widget::GridFitSize(double s) const
{
	if (s <= 0.0)
		return 0.0;
	double dpiscale = GetDpiScale();
	return std::max(std::floor(s * dpiscale + 0.25), 1.0) / dpiscale;
}

Size Widget::GetScreenSize()
{
	return DisplayWindow::GetScreenSize();
}

void* Widget::GetNativeHandle()
{
	Widget* w = Window();
	return w ? w->DispWindow->GetNativeHandle() : nullptr;
}

int Widget::GetNativePixelWidth()
{
	Widget* w = Window();
	return w ? w->DispWindow->GetPixelWidth() : 0;
}

int Widget::GetNativePixelHeight()
{
	Widget* w = Window();
	return w ? w->DispWindow->GetPixelHeight() : 0;
}

void Widget::SetStyleClass(const std::string& themeClass)
{
	if (StyleClass != themeClass)
	{
		StyleClass = themeClass;
		Update();
	}
}

void Widget::SetStyleState(const std::string& state)
{
	if (StyleState != state)
	{
		StyleState = state;
		Update();
	}
}

void Widget::SetStyleBool(const std::string& propertyName, bool value)
{
	StyleProperties[propertyName] = value;
}

void Widget::SetStyleInt(const std::string& propertyName, int value)
{
	StyleProperties[propertyName] = value;
}

void Widget::SetStyleDouble(const std::string& propertyName, double value)
{
	StyleProperties[propertyName] = value;
}

void Widget::SetStyleString(const std::string& propertyName, const std::string& value)
{
	StyleProperties[propertyName] = value;
}

void Widget::SetStyleColor(const std::string& propertyName, const Colorf& value)
{
	StyleProperties[propertyName] = value;
}

bool Widget::GetStyleBool(const std::string& propertyName) const
{
	auto it = StyleProperties.find(propertyName);
	if (it != StyleProperties.end())
		return std::get<bool>(it->second);
	WidgetStyle* style = WidgetTheme::GetTheme()->GetStyle(StyleClass);
	return style ? style->GetBool(StyleState, propertyName) : false;
}

int Widget::GetStyleInt(const std::string& propertyName) const
{
	auto it = StyleProperties.find(propertyName);
	if (it != StyleProperties.end())
		return std::get<int>(it->second);
	WidgetStyle* style = WidgetTheme::GetTheme()->GetStyle(StyleClass);
	return style ? style->GetInt(StyleState, propertyName) : 0;
}

double Widget::GetStyleDouble(const std::string& propertyName) const
{
	auto it = StyleProperties.find(propertyName);
	if (it != StyleProperties.end())
		return std::get<double>(it->second);
	WidgetStyle* style = WidgetTheme::GetTheme()->GetStyle(StyleClass);
	return style ? style->GetDouble(StyleState, propertyName) : 0.0;
}

std::string Widget::GetStyleString(const std::string& propertyName) const
{
	auto it = StyleProperties.find(propertyName);
	if (it != StyleProperties.end())
		return std::get<std::string>(it->second);
	WidgetStyle* style = WidgetTheme::GetTheme()->GetStyle(StyleClass);
	return style ? style->GetString(StyleState, propertyName) : std::string();
}

Colorf Widget::GetStyleColor(const std::string& propertyName) const
{
	auto it = StyleProperties.find(propertyName);
	if (it != StyleProperties.end())
		return std::get<Colorf>(it->second);
	WidgetStyle* style = WidgetTheme::GetTheme()->GetStyle(StyleClass);
	return style ? style->GetColor(StyleState, propertyName) : Colorf::transparent();
}
