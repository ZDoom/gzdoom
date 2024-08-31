
#include "core/canvas.h"
#include "core/rect.h"
#include "core/colorf.h"
#include "core/utf8reader.h"
#include "core/resourcedata.h"
#include "core/image.h"
#include "core/truetypefont.h"
#include "core/pathfill.h"
#include "window/window.h"
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cstring>

class CanvasTexture
{
public:
	int Width = 0;
	int Height = 0;
	std::vector<uint32_t> Data;
};

class CanvasGlyph
{
public:
	struct
	{
		double leftSideBearing = 0.0;
		double yOffset = 0.0;
		double advanceWidth = 0.0;
	} metrics;

	double u = 0.0;
	double v = 0.0;
	double uvwidth = 0.0f;
	double uvheight = 0.0f;
	std::shared_ptr<CanvasTexture> texture;
};

class CanvasFont
{
public:
	CanvasFont(const std::string& fontname, double height, std::vector<uint8_t>& _data) : fontname(fontname), height(height)
	{
		auto tdata = std::make_shared<TrueTypeFontFileData>(_data);
		ttf = std::make_unique<TrueTypeFont>(tdata);
		textmetrics = ttf->GetTextMetrics(height);
	}

	~CanvasFont()
	{
	}

	CanvasGlyph* getGlyph(uint32_t utfchar)
	{
		uint32_t glyphIndex = ttf->GetGlyphIndex(utfchar);
		if (glyphIndex == 0) return nullptr;

		auto& glyph = glyphs[glyphIndex];
		if (glyph)
			return glyph.get();

		glyph = std::make_unique<CanvasGlyph>();

		TrueTypeGlyph ttfglyph = ttf->LoadGlyph(glyphIndex, height);

		// Create final subpixel version
		int w = ttfglyph.width;
		int h = ttfglyph.height;
		int destwidth = (w + 2) / 3;
		auto texture = std::make_shared<CanvasTexture>();
		texture->Width = destwidth;
		texture->Height = h;
		texture->Data.resize(destwidth * h);

		uint8_t* grayscale = ttfglyph.grayscale.get();
		uint32_t* dest = (uint32_t*)texture->Data.data();
		for (int y = 0; y < h; y++)
		{
			uint8_t* sline = grayscale + y * w;
			uint32_t* dline = dest + y * destwidth;
			for (int x = 0; x < w; x += 3)
			{
				uint32_t values[5] =
				{
					x > 0 ? sline[x - 1] : 0U,
					sline[x],
					x + 1 < w ? sline[x + 1] : 0U,
					x + 2 < w ? sline[x + 2] : 0U,
					x + 3 < w ? sline[x + 3] : 0U
				};

				uint32_t red = (values[0] + values[1] + values[1] + values[2] + 2) >> 2;
				uint32_t green = (values[1] + values[2] + values[2] + values[3] + 2) >> 2;
				uint32_t blue = (values[2] + values[3] + values[3] + values[4] + 2) >> 2;
				uint32_t alpha = (red | green | blue) ? 255 : 0;

				*(dline++) = (alpha << 24) | (red << 16) | (green << 8) | blue;
			}
		}

		glyph->u = 0.0;
		glyph->v = 0.0;
		glyph->uvwidth = destwidth;
		glyph->uvheight = h;
		glyph->texture = std::move(texture);

		glyph->metrics.advanceWidth = (ttfglyph.advanceWidth + 2) / 3;
		glyph->metrics.leftSideBearing = (ttfglyph.leftSideBearing + 2) / 3;
		glyph->metrics.yOffset = ttfglyph.yOffset;

		return glyph.get();
	}

	std::unique_ptr<TrueTypeFont> ttf;

	std::string fontname;
	double height = 0.0;

	TrueTypeTextMetrics textmetrics;
	std::unordered_map<uint32_t, std::unique_ptr<CanvasGlyph>> glyphs;
};

class CanvasFontGroup
{
public:
	struct SingleFont
	{
		std::unique_ptr<CanvasFont> font;
		std::string language;
	};
	CanvasFontGroup(const std::string& fontname, double height) : height(height)
	{
		auto fontdata = LoadWidgetFontData(fontname);
		fonts.resize(fontdata.size());
		for (size_t i = 0; i < fonts.size(); i++)
		{
			fonts[i].font = std::make_unique<CanvasFont>(fontname, height, fontdata[i].fontdata);
			fonts[i].language = fontdata[i].language;
		}
	}

	CanvasGlyph* getGlyph(uint32_t utfchar, const char* lang = nullptr)
	{
		for (int i = 0; i < 2; i++)
		{
			for (auto& fd : fonts)
			{
				if (i == 1 || lang == nullptr || *lang == 0 || fd.language.empty() || fd.language == lang)
				{
					auto g = fd.font->getGlyph(utfchar);
					if (g) return g;
				}
			}
		}

		return nullptr;
	}

	TrueTypeTextMetrics& GetTextMetrics()
	{
		return fonts[0].font->textmetrics;
	}

	double height;
	std::vector<SingleFont> fonts;

};

class BitmapCanvas : public Canvas
{
public:
	BitmapCanvas(DisplayWindow* window);
	~BitmapCanvas();

	void begin(const Colorf& color) override;
	void end() override;

	void begin3d() override;
	void end3d() override;

	Point getOrigin() override;
	void setOrigin(const Point& origin) override;

	void pushClip(const Rect& box) override;
	void popClip() override;

	void fillRect(const Rect& box, const Colorf& color) override;
	void line(const Point& p0, const Point& p1, const Colorf& color) override;

	void drawText(const Point& pos, const Colorf& color, const std::string& text) override;
	Rect measureText(const std::string& text) override;
	VerticalTextPosition verticalTextAlign() override;

	void drawText(const std::shared_ptr<Font>& font, const Point& pos, const std::string& text, const Colorf& color) override { drawText(pos, color, text); }
	void drawTextEllipsis(const std::shared_ptr<Font>& font, const Point& pos, const Rect& clipBox, const std::string& text, const Colorf& color) override { drawText(pos, color, text); }
	Rect measureText(const std::shared_ptr<Font>& font, const std::string& text) override { return measureText(text); }
	FontMetrics getFontMetrics(const std::shared_ptr<Font>& font) override
	{
		VerticalTextPosition vtp = verticalTextAlign();
		FontMetrics metrics;
		metrics.external_leading = vtp.top;
		metrics.ascent = vtp.baseline - vtp.top;
		metrics.descent = vtp.bottom - vtp.baseline;
		metrics.height = metrics.ascent + metrics.descent;
		return metrics;
	}
	int getCharacterIndex(const std::shared_ptr<Font>& font, const std::string& text, const Point& hitPoint) override { return 0; }

	void drawImage(const std::shared_ptr<Image>& image, const Point& pos) override;

	void drawLineUnclipped(const Point& p0, const Point& p1, const Colorf& color);

	void drawTile(CanvasTexture* texture, float x, float y, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color);
	void drawGlyph(CanvasTexture* texture, float x, float y, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color);

	int getClipMinX() const;
	int getClipMinY() const;
	int getClipMaxX() const;
	int getClipMaxY() const;

	void setLanguage(const char* lang) { language = lang; }

	std::unique_ptr<CanvasTexture> createTexture(int width, int height, const void* pixels, ImageFormat format = ImageFormat::B8G8R8A8);

	template<typename T>
	static T clamp(T val, T minval, T maxval) { return std::max<T>(std::min<T>(val, maxval), minval); }

	DisplayWindow* window = nullptr;

	std::unique_ptr<CanvasFontGroup> font;
	std::unique_ptr<CanvasTexture> whiteTexture;

	Point origin;
	double uiscale = 1.0f;
	std::vector<Rect> clipStack;

	int width = 0;
	int height = 0;
	std::vector<uint32_t> pixels;

	std::unordered_map<std::shared_ptr<Image>, std::unique_ptr<CanvasTexture>> imageTextures;
	std::string language;
};

BitmapCanvas::BitmapCanvas(DisplayWindow* window) : window(window)
{
	uiscale = window->GetDpiScale();
	uint32_t white = 0xffffffff;
	whiteTexture = createTexture(1, 1, &white);
	font = std::make_unique<CanvasFontGroup>("NotoSans", 13.0 * uiscale);
}

BitmapCanvas::~BitmapCanvas()
{
}

Point BitmapCanvas::getOrigin()
{
	return origin;
}

void BitmapCanvas::setOrigin(const Point& newOrigin)
{
	origin = newOrigin;
}

void BitmapCanvas::pushClip(const Rect& box)
{
	if (!clipStack.empty())
	{
		const Rect& clip = clipStack.back();

		double x0 = box.x + origin.x;
		double y0 = box.y + origin.y;
		double x1 = x0 + box.width;
		double y1 = y0 + box.height;

		x0 = std::max(x0, clip.x);
		y0 = std::max(y0, clip.y);
		x1 = std::min(x1, clip.x + clip.width);
		y1 = std::min(y1, clip.y + clip.height);

		if (x0 < x1 && y0 < y1)
			clipStack.push_back(Rect::ltrb(x0, y0, x1, y1));
		else
			clipStack.push_back(Rect::xywh(0.0, 0.0, 0.0, 0.0));
	}
	else
	{
		clipStack.push_back(box);
	}
}

void BitmapCanvas::popClip()
{
	clipStack.pop_back();
}

void BitmapCanvas::fillRect(const Rect& box, const Colorf& color)
{
	drawTile(whiteTexture.get(), (float)((origin.x + box.x) * uiscale), (float)((origin.y + box.y) * uiscale), (float)(box.width * uiscale), (float)(box.height * uiscale), 0.0, 0.0, 1.0, 1.0, color);
}

void BitmapCanvas::drawImage(const std::shared_ptr<Image>& image, const Point& pos)
{
	auto& texture = imageTextures[image];
	if (!texture)
	{
		texture = createTexture(image->GetWidth(), image->GetHeight(), image->GetData(), image->GetFormat());
	}
	Colorf color(1.0f, 1.0f, 1.0f);
	drawTile(texture.get(), (float)((origin.x + pos.x) * uiscale), (float)((origin.y + pos.y) * uiscale), (float)(texture->Width * uiscale), (float)(texture->Height * uiscale), 0.0, 0.0, (float)texture->Width, (float)texture->Height, color);
}

void BitmapCanvas::line(const Point& p0, const Point& p1, const Colorf& color)
{
	double x0 = origin.x + p0.x;
	double y0 = origin.y + p0.y;
	double x1 = origin.x + p1.x;
	double y1 = origin.y + p1.y;

	if (clipStack.empty())// || (clipStack.back().contains({ x0, y0 }) && clipStack.back().contains({ x1, y1 })))
	{
		drawLineUnclipped({ x0, y0 }, { x1, y1 }, color);
	}
	else
	{
		const Rect& clip = clipStack.back();

		if (x0 > x1)
		{
			std::swap(x0, x1);
			std::swap(y0, y1);
		}

		if (x1 < clip.x || x0 >= clip.x + clip.width)
			return;

		// Clip left edge
		if (x0 < clip.x)
		{
			double dx = x1 - x0;
			double dy = y1 - y0;
			if (std::abs(dx) < 0.0001)
				return;
			y0 = y0 + (clip.x - x0) * dy / dx;
			x0 = clip.x;
		}

		// Clip right edge
		if (x1 > clip.x + clip.width)
		{
			double dx = x1 - x0;
			double dy = y1 - y0;
			if (std::abs(dx) < 0.0001)
				return;
			y1 = y1 + (clip.x + clip.width - x1) * dy / dx;
			x1 = clip.x + clip.width;
		}

		if (y0 > y1)
		{
			std::swap(x0, x1);
			std::swap(y0, y1);
		}

		if (y1 < clip.y || y0 >= clip.y + clip.height)
			return;

		// Clip top edge
		if (y0 < clip.y)
		{
			double dx = x1 - x0;
			double dy = y1 - y0;
			if (std::abs(dy) < 0.0001)
				return;
			x0 = x0 + (clip.y - y0) * dx / dy;
			y0 = clip.y;
		}

		// Clip bottom edge
		if (y1 > clip.y + clip.height)
		{
			double dx = x1 - x0;
			double dy = y1 - y0;
			if (std::abs(dy) < 0.0001)
				return;
			x1 = x1 + (clip.y + clip.height - y1) * dx / dy;
			y1 = clip.y + clip.height;
		}

		x0 = clamp(x0, clip.x, clip.x + clip.width);
		x1 = clamp(x1, clip.x, clip.x + clip.width);
		y0 = clamp(y0, clip.y, clip.y + clip.height);
		y1 = clamp(y1, clip.y, clip.y + clip.height);

		if (x0 != x1 || y0 != y1)
			drawLineUnclipped({ x0, y0 }, { x1, y1 }, color);
	}
}

void BitmapCanvas::drawText(const Point& pos, const Colorf& color, const std::string& text)
{
	double x = std::round((origin.x + pos.x) * uiscale);
	double y = std::round((origin.y + pos.y) * uiscale);

	UTF8Reader reader(text.data(), text.size());
	while (!reader.is_end())
	{
		CanvasGlyph* glyph = font->getGlyph(reader.character(), language.c_str());
		if (!glyph || !glyph->texture)
		{
			glyph = font->getGlyph(32);
		}

		if (glyph->texture)
		{
			double gx = std::round(x + glyph->metrics.leftSideBearing);
			double gy = std::round(y + glyph->metrics.yOffset);
			drawGlyph(glyph->texture.get(), (float)std::round(gx), (float)std::round(gy), (float)glyph->uvwidth, (float)glyph->uvheight, (float)glyph->u, (float)glyph->v, (float)glyph->uvwidth, (float)glyph->uvheight, color);
		}

		x += std::round(glyph->metrics.advanceWidth);
		reader.next();
	}
}

Rect BitmapCanvas::measureText(const std::string& text)
{
	double x = 0.0;
	double y = font->GetTextMetrics().ascender - font->GetTextMetrics().descender;

	UTF8Reader reader(text.data(), text.size());
	while (!reader.is_end())
	{
		CanvasGlyph* glyph = font->getGlyph(reader.character(), language.c_str());
		if (!glyph || !glyph->texture)
		{
			glyph = font->getGlyph(32);
		}

		x += std::round(glyph->metrics.advanceWidth);
		reader.next();
	}

	return Rect::xywh(0.0, 0.0, x / uiscale, y / uiscale);
}

VerticalTextPosition BitmapCanvas::verticalTextAlign()
{
	VerticalTextPosition align;
	align.top = 0.0f;
	auto tm = font->GetTextMetrics();
	align.baseline = tm.ascender / uiscale;
	align.bottom = (tm.ascender - tm.descender) / uiscale;
	return align;
}

std::unique_ptr<CanvasTexture> BitmapCanvas::createTexture(int width, int height, const void* pixels, ImageFormat format)
{
	auto texture = std::make_unique<CanvasTexture>();
	texture->Width = width;
	texture->Height = height;
	texture->Data.resize(width * height);
	if (format == ImageFormat::B8G8R8A8)
	{
		memcpy(texture->Data.data(), pixels, width * height * sizeof(uint32_t));
	}
	else
	{
		const uint32_t* src = (const uint32_t*)pixels;
		uint32_t* dest = texture->Data.data();
		int count = width * height;
		for (int i = 0; i < count; i++)
		{
			uint32_t a = (src[i] >> 24) & 0xff;
			uint32_t b = (src[i] >> 16) & 0xff;
			uint32_t g = (src[i] >> 8) & 0xff;
			uint32_t r = src[i] & 0xff;
			dest[i] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
	return texture;
}

void BitmapCanvas::drawLineUnclipped(const Point& p0, const Point& p1, const Colorf& color)
{
	if (p0.x == p1.x)
	{
		drawTile(whiteTexture.get(), (float)((p0.x - 0.5) * uiscale), (float)(p0.y * uiscale), (float)((p1.x + 0.5) * uiscale), (float)(p1.y * uiscale), 0.0f, 0.0f, 1.0f, 1.0f, color);
	}
	else if (p0.y == p1.y)
	{
		drawTile(whiteTexture.get(), (float)(p0.x * uiscale), (float)((p0.y - 0.5) * uiscale), (float)(p1.x * uiscale), (float)((p1.y + 0.5) * uiscale), 0.0f, 0.0f, 1.0f, 1.0f, color);
	}
	else
	{
		// To do: draw line using bresenham
	}
}

int BitmapCanvas::getClipMinX() const
{
	return clipStack.empty() ? 0 : (int)std::max(clipStack.back().x * uiscale, 0.0);
}

int BitmapCanvas::getClipMinY() const
{
	return clipStack.empty() ? 0 : (int)std::max(clipStack.back().y * uiscale, 0.0);
}

int BitmapCanvas::getClipMaxX() const
{
	return clipStack.empty() ? width : (int)std::min((clipStack.back().x + clipStack.back().width) * uiscale, (double)width);
}

int BitmapCanvas::getClipMaxY() const
{
	return clipStack.empty() ? height : (int)std::min((clipStack.back().y + clipStack.back().height) * uiscale, (double)height);
}

void BitmapCanvas::drawTile(CanvasTexture* texture, float left, float top, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color)
{
	if (width <= 0.0f || height <= 0.0f)
		return;

	int swidth = texture->Width;
	int sheight = texture->Height;
	const uint32_t* src = texture->Data.data();

	int dwidth = this->width;
	int dheight = this->height;
	uint32_t* dest = this->pixels.data();

	int x0 = (int)left;
	int x1 = (int)(left + width);
	int y0 = (int)top;
	int y1 = (int)(top + height);

	x0 = std::max(x0, getClipMinX());
	y0 = std::max(y0, getClipMinY());
	x1 = std::min(x1, getClipMaxX());
	y1 = std::min(y1, getClipMaxY());
	if (x1 <= x0 || y1 <= y0)
		return;

	uint32_t cred = (int32_t)clamp(color.r * 256.0f, 0.0f, 256.0f);
	uint32_t cgreen = (int32_t)clamp(color.g * 256.0f, 0.0f, 256.0f);
	uint32_t cblue = (int32_t)clamp(color.b * 256.0f, 0.0f, 256.0f);
	uint32_t calpha = (int32_t)clamp(color.a * 256.0f, 0.0f, 256.0f);

	float uscale = uvwidth / width;
	float vscale = uvheight / height;

	for (int y = y0; y < y1; y++)
	{
		float vpix = v + vscale * (y + 0.5f - top);
		const uint32_t* sline = src + ((int)vpix) * swidth;

		uint32_t* dline = dest + y * dwidth;
		for (int x = x0; x < x1; x++)
		{
			float upix = u + uscale * (x + 0.5f - left);
			uint32_t spixel = sline[(int)upix];
			uint32_t salpha = spixel >> 24;
			uint32_t sred = (spixel >> 16) & 0xff;
			uint32_t sgreen = (spixel >> 8) & 0xff;
			uint32_t sblue = spixel & 0xff;

			uint32_t dpixel = dline[x];
			uint32_t dalpha = dpixel >> 24;
			uint32_t dred = (dpixel >> 16) & 0xff;
			uint32_t dgreen = (dpixel >> 8) & 0xff;
			uint32_t dblue = dpixel & 0xff;

			// Pixel shade
			sred = (cred * sred + 127) >> 8;
			sgreen = (cgreen * sgreen + 127) >> 8;
			sblue = (cblue * sblue + 127) >> 8;
			salpha = (calpha * salpha + 127) >> 8;

			// Rescale from [0,255] to [0,256]
			uint32_t sa = salpha + (salpha >> 7);
			uint32_t sinva = 256 - sa;

			// dest.rgba = color.rgba * src.rgba * src.a + dest.rgba * (1-src.a)
			uint32_t a = (salpha * sa + dalpha * sinva + 127) >> 8;
			uint32_t r = (sred * sa + dred * sinva + 127) >> 8;
			uint32_t g = (sgreen * sa + dgreen * sinva + 127) >> 8;
			uint32_t b = (sblue * sa + dblue * sinva + 127) >> 8;
			dline[x] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
}

void BitmapCanvas::drawGlyph(CanvasTexture* texture, float left, float top, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color)
{
	if (width <= 0.0f || height <= 0.0f)
		return;

	int swidth = texture->Width;
	int sheight = texture->Height;
	const uint32_t* src = texture->Data.data();

	int dwidth = this->width;
	int dheight = this->height;
	uint32_t* dest = this->pixels.data();

	int x0 = (int)left;
	int x1 = (int)(left + width);
	int y0 = (int)top;
	int y1 = (int)(top + height);

	x0 = std::max(x0, getClipMinX());
	y0 = std::max(y0, getClipMinY());
	x1 = std::min(x1, getClipMaxX());
	y1 = std::min(y1, getClipMaxY());
	if (x1 <= x0 || y1 <= y0)
		return;

	uint32_t cred = (int32_t)clamp(color.r * 255.0f, 0.0f, 255.0f);
	uint32_t cgreen = (int32_t)clamp(color.g * 255.0f, 0.0f, 255.0f);
	uint32_t cblue = (int32_t)clamp(color.b * 255.0f, 0.0f, 255.0f);

	float uscale = uvwidth / width;
	float vscale = uvheight / height;

	for (int y = y0; y < y1; y++)
	{
		float vpix = v + vscale * (y + 0.5f - top);
		const uint32_t* sline = src + ((int)vpix) * swidth;

		uint32_t* dline = dest + y * dwidth;
		for (int x = x0; x < x1; x++)
		{
			float upix = u + uscale * (x + 0.5f - left);
			uint32_t spixel = sline[(int)upix];
			uint32_t sred = (spixel >> 16) & 0xff;
			uint32_t sgreen = (spixel >> 8) & 0xff;
			uint32_t sblue = spixel & 0xff;

			uint32_t dpixel = dline[x];
			uint32_t dred = (dpixel >> 16) & 0xff;
			uint32_t dgreen = (dpixel >> 8) & 0xff;
			uint32_t dblue = dpixel & 0xff;

			// Rescale from [0,255] to [0,256]
			sred += sred >> 7;
			sgreen += sgreen >> 7;
			sblue += sblue >> 7;

			// dest.rgb = color.rgb * src.rgb + dest.rgb * (1-src.rgb)
			uint32_t r = (cred * sred + dred * (256 - sred) + 127) >> 8;
			uint32_t g = (cgreen * sgreen + dgreen * (256 - sgreen) + 127) >> 8;
			uint32_t b = (cblue * sblue + dblue * (256 - sblue) + 127) >> 8;
			dline[x] = 0xff000000 | (r << 16) | (g << 8) | b;
		}
	}
}

void BitmapCanvas::begin(const Colorf& color)
{
	uiscale = window->GetDpiScale();

	uint32_t r = (int32_t)clamp(color.r * 255.0f, 0.0f, 255.0f);
	uint32_t g = (int32_t)clamp(color.g * 255.0f, 0.0f, 255.0f);
	uint32_t b = (int32_t)clamp(color.b * 255.0f, 0.0f, 255.0f);
	uint32_t a = (int32_t)clamp(color.a * 255.0f, 0.0f, 255.0f);
	uint32_t bgcolor = (a << 24) | (r << 16) | (g << 8) | b;
	width = window->GetPixelWidth();
	height = window->GetPixelHeight();
	pixels.clear();
	pixels.resize(width * height, bgcolor);
}

void BitmapCanvas::end()
{
	window->PresentBitmap(width, height, pixels.data());
}

void BitmapCanvas::begin3d()
{
}

void BitmapCanvas::end3d()
{
}

/////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Canvas> Canvas::create(DisplayWindow* window)
{
	return std::make_unique<BitmapCanvas>(window);
}
