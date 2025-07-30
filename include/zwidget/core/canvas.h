#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "image.h"
#include "rect.h"
#include <vector>

class Font;
class Point;
class Rect;
class Colorf;
class DisplayWindow;
class CanvasFontGroup;

class CanvasTexture
{
public:
	virtual ~CanvasTexture() = default;

	int Width = 0;
	int Height = 0;
};

class FontMetrics
{
public:
	double ascent = 0.0;
	double descent = 0.0;
	double external_leading = 0.0;
	double height = 0.0;
};

struct VerticalTextPosition
{
	double top = 0.0;
	double baseline = 0.0;
	double bottom = 0.0;
};

class Canvas
{
public:
	static std::unique_ptr<Canvas> create();

	Canvas();
	virtual ~Canvas();

	virtual void attach(DisplayWindow* window);
	virtual void detach();

	virtual void begin(const Colorf& color);
	virtual void end() { }

	virtual void begin3d() { }
	virtual void end3d() { }

	Point getOrigin();
	void setOrigin(const Point& origin);

	void pushClip(const Rect& box);
	void popClip();

	void fillRect(const Rect& box, const Colorf& color);
	void line(const Point& p0, const Point& p1, const Colorf& color);

	void drawText(const Point& pos, const Colorf& color, const std::string& text);
	Rect measureText(const std::string& text);
	VerticalTextPosition verticalTextAlign();

	void drawText(const std::shared_ptr<Font>& font, const Point& pos, const std::string& text, const Colorf& color);
	void drawTextEllipsis(const std::shared_ptr<Font>& font, const Point& pos, const Rect& clipBox, const std::string& text, const Colorf& color);
	Rect measureText(const std::shared_ptr<Font>& font, const std::string& text);
	FontMetrics getFontMetrics(const std::shared_ptr<Font>& font);
	int getCharacterIndex(const std::shared_ptr<Font>& font, const std::string& text, const Point& hitPoint);

	void drawImage(const std::shared_ptr<Image>& image, const Point& pos);
	void drawImage(const std::shared_ptr<Image>& image, const Rect& box);

protected:
	virtual std::unique_ptr<CanvasTexture> createTexture(int width, int height, const void* pixels, ImageFormat format = ImageFormat::B8G8R8A8) = 0;
	virtual void drawLineAntialiased(float x0, float y0, float x1, float y1, Colorf color) = 0;
	virtual void fillTile(float x, float y, float width, float height, Colorf color) = 0;
	virtual void drawTile(CanvasTexture* texture, float x, float y, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color) = 0;
	virtual void drawGlyph(CanvasTexture* texture, float x, float y, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color) = 0;

	int getClipMinX() const;
	int getClipMinY() const;
	int getClipMaxX() const;
	int getClipMaxY() const;

	template<typename T>
	static T clamp(T val, T minval, T maxval) { return std::max<T>(std::min<T>(val, maxval), minval); }

	DisplayWindow* window = nullptr;
	int width = 0;
	int height = 0;
	double uiscale = 1.0f;

	std::unique_ptr<CanvasTexture> whiteTexture;

private:
	void setLanguage(const char* lang) { language = lang; }
	void drawLineUnclipped(const Point& p0, const Point& p1, const Colorf& color);

	std::unique_ptr<CanvasFontGroup> font;

	Point origin;
	std::vector<Rect> clipStack;

	std::unordered_map<std::shared_ptr<Image>, std::unique_ptr<CanvasTexture>> imageTextures;
	std::string language;

	friend class CanvasFont;
};
