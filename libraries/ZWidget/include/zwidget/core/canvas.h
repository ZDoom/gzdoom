#pragma once

#include <memory>
#include <string>

class Font;
class Image;
class Point;
class Rect;
class Colorf;
class DisplayWindow;
struct VerticalTextPosition;

class FontMetrics
{
public:
	double ascent = 0.0;
	double descent = 0.0;
	double external_leading = 0.0;
	double height = 0.0;
};

class Canvas
{
public:
	static std::unique_ptr<Canvas> create(DisplayWindow* window);

	virtual ~Canvas() = default;

	virtual void begin(const Colorf& color) = 0;
	virtual void end() = 0;

	virtual void begin3d() = 0;
	virtual void end3d() = 0;

	virtual Point getOrigin() = 0;
	virtual void setOrigin(const Point& origin) = 0;

	virtual void pushClip(const Rect& box) = 0;
	virtual void popClip() = 0;

	virtual void fillRect(const Rect& box, const Colorf& color) = 0;
	virtual void line(const Point& p0, const Point& p1, const Colorf& color) = 0;

	virtual void drawText(const Point& pos, const Colorf& color, const std::string& text) = 0;
	virtual Rect measureText(const std::string& text) = 0;
	virtual VerticalTextPosition verticalTextAlign() = 0;

	virtual void drawText(const std::shared_ptr<Font>& font, const Point& pos, const std::string& text, const Colorf& color) = 0;
	virtual void drawTextEllipsis(const std::shared_ptr<Font>& font, const Point& pos, const Rect& clipBox, const std::string& text, const Colorf& color) = 0;
	virtual Rect measureText(const std::shared_ptr<Font>& font, const std::string& text) = 0;
	virtual FontMetrics getFontMetrics(const std::shared_ptr<Font>& font) = 0;
	virtual int getCharacterIndex(const std::shared_ptr<Font>& font, const std::string& text, const Point& hitPoint) = 0;

	virtual void drawImage(const std::shared_ptr<Image>& image, const Point& pos) = 0;
};

struct VerticalTextPosition
{
	double top = 0.0;
	double baseline = 0.0;
	double bottom = 0.0;
};
