
#include "core/theme.h"
#include "core/widget.h"
#include "core/canvas.h"

void WidgetStyle::SetBool(const std::string& state, const std::string& propertyName, bool value)
{
	StyleProperties[state][propertyName] = value;
}

void WidgetStyle::SetInt(const std::string& state, const std::string& propertyName, int value)
{
	StyleProperties[state][propertyName] = value;
}

void WidgetStyle::SetDouble(const std::string& state, const std::string& propertyName, double value)
{
	StyleProperties[state][propertyName] = value;
}

void WidgetStyle::SetString(const std::string& state, const std::string& propertyName, const std::string& value)
{
	StyleProperties[state][propertyName] = value;
}

void WidgetStyle::SetColor(const std::string& state, const std::string& propertyName, const Colorf& value)
{
	StyleProperties[state][propertyName] = value;
}

const WidgetStyle::PropertyVariant* WidgetStyle::FindProperty(const std::string& state, const std::string& propertyName) const
{
	const WidgetStyle* style = this;
	do
	{
		// Look for property in the specific state
		auto stateIt = style->StyleProperties.find(state);
		if (stateIt != style->StyleProperties.end())
		{
			auto it = stateIt->second.find(propertyName);
			if (it != stateIt->second.end())
				return &it->second;
		}

		// Fall back to the widget main style
		if (state != std::string())
		{
			stateIt = style->StyleProperties.find(std::string());
			if (stateIt != style->StyleProperties.end())
			{
				auto it = stateIt->second.find(propertyName);
				if (it != stateIt->second.end())
					return &it->second;
			}
		}

		style = style->ParentStyle;
	} while (style);
	return nullptr;
}

bool WidgetStyle::GetBool(const std::string& state, const std::string& propertyName) const
{
	const PropertyVariant* prop = FindProperty(state, propertyName);
	return prop ? std::get<bool>(*prop) : false;
}

int WidgetStyle::GetInt(const std::string& state, const std::string& propertyName) const
{
	const PropertyVariant* prop = FindProperty(state, propertyName);
	return prop ? std::get<int>(*prop) : 0;
}

double WidgetStyle::GetDouble(const std::string& state, const std::string& propertyName) const
{
	const PropertyVariant* prop = FindProperty(state, propertyName);
	return prop ? std::get<double>(*prop) : 0.0;
}

std::string WidgetStyle::GetString(const std::string& state, const std::string& propertyName) const
{
	const PropertyVariant* prop = FindProperty(state, propertyName);
	return prop ? std::get<std::string>(*prop) : std::string();
}

Colorf WidgetStyle::GetColor(const std::string& state, const std::string& propertyName) const
{
	const PropertyVariant* prop = FindProperty(state, propertyName);
	return prop ? std::get<Colorf>(*prop) : Colorf::transparent();
}

/////////////////////////////////////////////////////////////////////////////

void BasicWidgetStyle::Paint(Widget* widget, Canvas* canvas, Size size)
{
	Colorf bgcolor = widget->GetStyleColor("background-color");
	if (bgcolor.a > 0.0f)
		canvas->fillRect(Rect::xywh(0.0, 0.0, size.width, size.height), bgcolor);

	Colorf borderleft = widget->GetStyleColor("border-left-color");
	Colorf bordertop = widget->GetStyleColor("border-top-color");
	Colorf borderright = widget->GetStyleColor("border-right-color");
	Colorf borderbottom = widget->GetStyleColor("border-bottom-color");

	double borderwidth = widget->GridFitSize(1.0);

	if (bordertop.a > 0.0f)
		canvas->fillRect(Rect::xywh(0.0, 0.0, size.width, borderwidth), bordertop);
	if (borderbottom.a > 0.0f)
		canvas->fillRect(Rect::xywh(0.0, size.height - borderwidth, size.width, borderwidth), borderbottom);
	if (borderleft.a > 0.0f)
		canvas->fillRect(Rect::xywh(0.0, 0.0, borderwidth, size.height), borderleft);
	if (borderright.a > 0.0f)
		canvas->fillRect(Rect::xywh(size.width - borderwidth, 0.0, borderwidth, size.height), borderright);
}

/////////////////////////////////////////////////////////////////////////////

static std::unique_ptr<WidgetTheme> CurrentTheme;

WidgetStyle* WidgetTheme::RegisterStyle(std::unique_ptr<WidgetStyle> widgetStyle, const std::string& widgetClass)
{
	auto& style = Styles[widgetClass];
	style = std::move(widgetStyle);
	return style.get();
}

WidgetStyle* WidgetTheme::GetStyle(const std::string& widgetClass)
{
	auto it = Styles.find(widgetClass);
	return it != Styles.end() ? it->second.get() : nullptr;
}

void WidgetTheme::SetTheme(std::unique_ptr<WidgetTheme> theme)
{
	CurrentTheme = std::move(theme);
}

WidgetTheme* WidgetTheme::GetTheme()
{
	return CurrentTheme.get();
}

WidgetTheme::WidgetTheme(const struct SimpleTheme &theme)
{

	auto bgMain   = theme.bgMain;   // background
	auto fgMain   = theme.fgMain;   //
	auto bgLight  = theme.bgLight;  // headers / inputs
	auto fgLight  = theme.fgLight;  //
	auto bgAction = theme.bgAction; // interactive elements
	auto fgAction = theme.fgAction; //
	auto bgHover  = theme.bgHover;  // hover / highlight
	auto fgHover  = theme.fgHover;  //
	auto bgActive = theme.bgActive; // click
	auto fgActive = theme.fgActive; //
	auto border   = theme.border;   // around elements
	auto divider  = theme.divider;  // between elements

	auto none   = Colorf::transparent();

	auto widget = RegisterStyle(std::make_unique<BasicWidgetStyle>(), "widget");
	/*auto textlabel =*/ RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "textlabel");
	auto pushbutton = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "pushbutton");
	auto lineedit = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "lineedit");
	auto textedit = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "textedit");
	auto listview = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "listview");
	auto dropdown = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "dropdown");
	auto scrollbar = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "scrollbar");
	auto tabbar = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "tabbar");
	auto tabbar_tab = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "tabbar-tab");
	auto tabbar_spacer = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "tabbar-spacer");
	auto tabwidget_stack = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "tabwidget-stack");
	auto checkbox_label = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "checkbox-label");
	auto menubar = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "menubar");
	auto menubaritem = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "menubaritem");
	auto menu = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "menu");
	auto menuitem = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "menuitem");
	auto toolbar = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "toolbar");
	auto toolbarbutton = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "toolbarbutton");
	auto statusbar = RegisterStyle(std::make_unique<BasicWidgetStyle>(widget), "statusbar");

	widget->SetString("font-family", "NotoSans");
	widget->SetColor("color", fgMain);
	widget->SetColor("window-background", bgMain);
	widget->SetColor("window-border", bgMain);
	widget->SetColor("window-caption-color", bgLight);
	widget->SetColor("window-caption-text-color", fgLight);

	pushbutton->SetDouble("noncontent-left", 10.0);
	pushbutton->SetDouble("noncontent-top", 5.0);
	pushbutton->SetDouble("noncontent-right", 10.0);
	pushbutton->SetDouble("noncontent-bottom", 5.0);
	pushbutton->SetColor("color", fgAction);
	pushbutton->SetColor("background-color", bgAction);
	pushbutton->SetColor("border-left-color", border);
	pushbutton->SetColor("border-top-color", border);
	pushbutton->SetColor("border-right-color", border);
	pushbutton->SetColor("border-bottom-color", border);
	pushbutton->SetColor("hover", "color", fgHover);
	pushbutton->SetColor("hover", "background-color", bgHover);
	pushbutton->SetColor("down", "color", fgActive);
	pushbutton->SetColor("down", "background-color", bgActive);

	lineedit->SetDouble("noncontent-left", 5.0);
	lineedit->SetDouble("noncontent-top", 3.0);
	lineedit->SetDouble("noncontent-right", 5.0);
	lineedit->SetDouble("noncontent-bottom", 3.0);
	lineedit->SetColor("color", fgLight);
	lineedit->SetColor("background-color", bgLight);
	lineedit->SetColor("border-left-color", border);
	lineedit->SetColor("border-top-color", border);
	lineedit->SetColor("border-right-color", border);
	lineedit->SetColor("border-bottom-color", border);
	lineedit->SetColor("selection-color", bgHover);
	lineedit->SetColor("no-focus-selection-color", bgHover);

	textedit->SetDouble("noncontent-left", 8.0);
	textedit->SetDouble("noncontent-top", 8.0);
	textedit->SetDouble("noncontent-right", 8.0);
	textedit->SetDouble("noncontent-bottom", 8.0);
	textedit->SetColor("color", fgLight);
	textedit->SetColor("background-color", bgLight);
	textedit->SetColor("border-left-color", border);
	textedit->SetColor("border-top-color", border);
	textedit->SetColor("border-right-color", border);
	textedit->SetColor("border-bottom-color", border);
	textedit->SetColor("selection-color", bgHover);

	listview->SetDouble("noncontent-left", 10.0);
	listview->SetDouble("noncontent-top", 10.0);
	listview->SetDouble("noncontent-right", 3.0);
	listview->SetDouble("noncontent-bottom", 10.0);
	listview->SetColor("color", fgLight);
	listview->SetColor("background-color", bgLight);
	listview->SetColor("border-left-color", border);
	listview->SetColor("border-top-color", border);
	listview->SetColor("border-right-color", border);
	listview->SetColor("border-bottom-color", border);
	listview->SetColor("selection-color", bgHover);

	dropdown->SetDouble("noncontent-left", 5.0);
	dropdown->SetDouble("noncontent-top", 5.0);
	dropdown->SetDouble("noncontent-right", 5.0);
	dropdown->SetDouble("noncontent-bottom", 5.0);
	dropdown->SetColor("color", fgLight);
	dropdown->SetColor("background-color", bgLight);
	dropdown->SetColor("border-left-color", border);
	dropdown->SetColor("border-top-color", border);
	dropdown->SetColor("border-right-color", border);
	dropdown->SetColor("border-bottom-color", border);
	dropdown->SetColor("arrow-color", border);

	scrollbar->SetColor("track-color", divider);
	scrollbar->SetColor("thumb-color", border);

	tabbar->SetDouble("spacer-left", 20.0);
	tabbar->SetDouble("spacer-right", 20.0);
	tabbar->SetColor("background-color", bgLight);

	tabbar_tab->SetDouble("noncontent-left", 15.0);
	tabbar_tab->SetDouble("noncontent-right", 15.0);
	tabbar_tab->SetDouble("noncontent-top", 1.0);
	tabbar_tab->SetDouble("noncontent-bottom", 1.0);
	tabbar_tab->SetColor("color", fgMain);
	tabbar_tab->SetColor("background-color", bgMain);
	tabbar_tab->SetColor("border-left-color", divider);
	tabbar_tab->SetColor("border-top-color", divider);
	tabbar_tab->SetColor("border-right-color", divider);
	tabbar_tab->SetColor("border-bottom-color", border);
	tabbar_tab->SetColor("hover", "color", fgAction);
	tabbar_tab->SetColor("hover", "background-color", bgAction);
	tabbar_tab->SetColor("active", "background-color", bgMain);
	tabbar_tab->SetColor("active", "border-left-color", border);
	tabbar_tab->SetColor("active", "border-top-color", border);
	tabbar_tab->SetColor("active", "border-right-color", border);
	tabbar_tab->SetColor("active", "border-bottom-color", none);

	tabbar_spacer->SetDouble("noncontent-bottom", 1.0);
	tabbar_spacer->SetColor("border-bottom-color", border);

	tabwidget_stack->SetDouble("noncontent-left", 20.0);
	tabwidget_stack->SetDouble("noncontent-top", 5.0);
	tabwidget_stack->SetDouble("noncontent-right", 20.0);
	tabwidget_stack->SetDouble("noncontent-bottom", 5.0);

	checkbox_label->SetColor("checked-outer-border-color", border);
	checkbox_label->SetColor("checked-inner-border-color", bgMain);
	checkbox_label->SetColor("checked-color", fgMain);
	checkbox_label->SetColor("unchecked-outer-border-color", border);
	checkbox_label->SetColor("unchecked-inner-border-color", bgMain);

	menubar->SetColor("background-color", bgLight);
	toolbar->SetColor("background-color", bgLight);
	statusbar->SetColor("background-color", bgLight);

	toolbarbutton->SetColor("hover", "color", fgHover);
	toolbarbutton->SetColor("hover", "background-color", bgHover);
	toolbarbutton->SetColor("down", "color", fgActive);
	toolbarbutton->SetColor("down", "background-color", bgActive);

	menubaritem->SetColor("color", fgMain);
	menubaritem->SetColor("hover", "color", fgHover);
	menubaritem->SetColor("hover", "background-color", bgHover);
	menubaritem->SetColor("down", "color", fgActive);
	menubaritem->SetColor("down", "background-color", bgActive);

	menu->SetDouble("noncontent-left", 5.0);
	menu->SetDouble("noncontent-top", 5.0);
	menu->SetDouble("noncontent-right", 5.0);
	menu->SetDouble("noncontent-bottom", 5.0);
	menu->SetColor("color", fgMain);
	menu->SetColor("background-color", bgMain);
	menu->SetColor("border-left-color", border);
	menu->SetColor("border-top-color", border);
	menu->SetColor("border-right-color", border);
	menu->SetColor("border-bottom-color", border);

	menuitem->SetColor("hover", "color", fgHover);
	menuitem->SetColor("hover", "background-color", bgHover);
	menuitem->SetColor("down", "color", fgActive);
	menuitem->SetColor("down", "background-color", bgActive);
}

/////////////////////////////////////////////////////////////////////////////

DarkWidgetTheme::DarkWidgetTheme(): WidgetTheme({
	Colorf::fromRgb(0x2A2A2A), // background
	Colorf::fromRgb(0xE2DFDB), //
	Colorf::fromRgb(0x212121), // headers / inputs
	Colorf::fromRgb(0xE2DFDB), //
	Colorf::fromRgb(0x444444), // interactive elements
	Colorf::fromRgb(0xFFFFFF), //
	Colorf::fromRgb(0xC83C00), // hover / highlight
	Colorf::fromRgb(0xFFFFFF), //
	Colorf::fromRgb(0xBBBBBB), // click
	Colorf::fromRgb(0x000000), //
	Colorf::fromRgb(0x646464), // around elements
	Colorf::fromRgb(0x555555)  // between elements
}) {};

/////////////////////////////////////////////////////////////////////////////

LightWidgetTheme::LightWidgetTheme(): WidgetTheme({
	Colorf::fromRgb(0xF0F0F0), // background
	Colorf::fromRgb(0x191919), //
	Colorf::fromRgb(0xFAFAFA), // headers / inputs
	Colorf::fromRgb(0x191919), //
	Colorf::fromRgb(0xC8C8C8), // interactive elements
	Colorf::fromRgb(0x000000), //
	Colorf::fromRgb(0xD2D2FF), // hover / highlight
	Colorf::fromRgb(0x000000), //
	Colorf::fromRgb(0xC7B4FF), // click
	Colorf::fromRgb(0x000000), //
	Colorf::fromRgb(0xA0A0A0), // around elements
	Colorf::fromRgb(0xB9B9B9)  // between elements
}) {};
