#pragma once

#include <memory>
#include <string>
#include <variant>
#include <unordered_map>
#include "rect.h"
#include "colorf.h"

class Widget;
class Canvas;

class WidgetStyle
{
public:
	WidgetStyle(WidgetStyle* parentStyle = nullptr) : ParentStyle(parentStyle) { }
	virtual ~WidgetStyle() = default;
	virtual void Paint(Widget* widget, Canvas* canvas, Size size) = 0;

	void SetBool(const std::string& state, const std::string& propertyName, bool value);
	void SetInt(const std::string& state, const std::string& propertyName, int value);
	void SetDouble(const std::string& state, const std::string& propertyName, double value);
	void SetString(const std::string& state, const std::string& propertyName, const std::string& value);
	void SetColor(const std::string& state, const std::string& propertyName, const Colorf& value);

	void SetBool(const std::string& propertyName, bool value) { SetBool(std::string(), propertyName, value); }
	void SetInt(const std::string& propertyName, int value) { SetInt(std::string(), propertyName, value); }
	void SetDouble(const std::string& propertyName, double value) { SetDouble(std::string(), propertyName, value); }
	void SetString(const std::string& propertyName, const std::string& value) { SetString(std::string(), propertyName, value); }
	void SetColor(const std::string& propertyName, const Colorf& value) { SetColor(std::string(), propertyName, value); }

private:
	// Note: do not call these directly. Use widget->GetStyleXX instead since a widget may explicitly override a class style
	bool GetBool(const std::string& state, const std::string& propertyName) const;
	int GetInt(const std::string& state, const std::string& propertyName) const;
	double GetDouble(const std::string& state, const std::string& propertyName) const;
	std::string GetString(const std::string& state, const std::string& propertyName) const;
	Colorf GetColor(const std::string& state, const std::string& propertyName) const;

	WidgetStyle* ParentStyle = nullptr;
	typedef std::variant<bool, int, double, std::string, Colorf> PropertyVariant;
	std::unordered_map<std::string, std::unordered_map<std::string, PropertyVariant>> StyleProperties;

	const PropertyVariant* FindProperty(const std::string& state, const std::string& propertyName) const;

	friend class Widget;
};

class BasicWidgetStyle : public WidgetStyle
{
public:
	using WidgetStyle::WidgetStyle;
	void Paint(Widget* widget, Canvas* canvas, Size size) override;
};

class WidgetTheme
{
public:
	virtual ~WidgetTheme() = default;
	
	WidgetStyle* RegisterStyle(std::unique_ptr<WidgetStyle> widgetStyle, const std::string& widgetClass);
	WidgetStyle* GetStyle(const std::string& widgetClass);

	static void SetTheme(std::unique_ptr<WidgetTheme> theme);
	static WidgetTheme* GetTheme();

private:
	std::unordered_map<std::string, std::unique_ptr<WidgetStyle>> Styles;
};

class DarkWidgetTheme : public WidgetTheme
{
public:
	DarkWidgetTheme();
};

class LightWidgetTheme : public WidgetTheme
{
public:
	LightWidgetTheme();
};
