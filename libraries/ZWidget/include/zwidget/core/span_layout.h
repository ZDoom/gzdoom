#pragma once

#include <vector>
#include <algorithm>

#include "colorf.h"
#include "rect.h"
#include "font.h"
#include "canvas.h"

class Widget;
class Image;
class Canvas;

enum SpanAlign
{
	span_left,
	span_right,
	span_center,
	span_justify
};

class SpanLayout
{
public:
	SpanLayout();
	~SpanLayout();

	struct HitTestResult
	{
		enum Type
		{
			no_objects_available,
			outside_top,
			outside_left,
			outside_right,
			outside_bottom,
			inside
		};
		
		Type type = {};
		int object_id = -1;
		size_t offset = 0;
	};

	void Clear();

	void AddText(const std::string& text, std::shared_ptr<Font> font, const Colorf& color = Colorf(), int id = -1);
	void AddImage(const std::shared_ptr<Image> image, double baseline_offset = 0, int id = -1);
	void AddWidget(Widget* component, double baseline_offset = 0, int id = -1);

	void Layout(Canvas* canvas, double max_width);

	void SetPosition(const Point& pos);

	Size GetSize() const;
	Rect GetRect() const;

	std::vector<Rect> GetRectById(int id) const;

	HitTestResult HitTest(Canvas* canvas, const Point& pos);

	void DrawLayout(Canvas* canvas);

	/// Draw layout generating ellipsis for clipped text
	void DrawLayoutEllipsis(Canvas* canvas, const Rect& content_rect);

	void SetComponentGeometry();

	Size FindPreferredSize(Canvas* canvas);

	void SetSelectionRange(std::string::size_type start, std::string::size_type end);
	void SetSelectionColors(const Colorf& foreground, const Colorf& background);

	void ShowCursor();
	void HideCursor();

	void SetCursorPos(std::string::size_type pos);
	void SetCursorOverwriteMode(bool enable);
	void SetCursorColor(const Colorf& color);

	std::string GetCombinedText() const;

	void SetAlign(SpanAlign align);

	double GetFirstBaselineOffset();
	double GetLastBaselineOffset();

private:
	struct TextBlock
	{
		size_t start = 0;
		size_t end = 0;
	};

	enum ObjectType
	{
		object_text,
		object_image,
		object_component
	};

	enum FloatType
	{
		float_none,
		float_left,
		float_right
	};

	struct SpanObject
	{
		ObjectType type = object_text;
		FloatType float_type = float_none;

		std::shared_ptr<Font> font;
		Colorf color;
		size_t start = 0, end = 0;

		std::shared_ptr<Image> image;
		Widget* component = nullptr;
		double baseline_offset = 0;

		int id = -1;
	};

	struct LineSegment
	{
		ObjectType type = object_text;

		std::shared_ptr<Font> font;
		Colorf color;
		size_t start = 0;
		size_t end = 0;
		double ascender = 0;
		double descender = 0;

		double x_position = 0;
		double width = 0;

		std::shared_ptr<Image> image;
		Widget* component = nullptr;
		double baseline_offset = 0;

		int id = -1;
	};

	struct Line
	{
		double width = 0;	// Width of the entire line (including spaces)
		double height = 0;
		double ascender = 0;
		std::vector<LineSegment> segments;
	};

	struct TextSizeResult
	{
		size_t start = 0;
		size_t end = 0;
		double width = 0;
		double height = 0;
		double ascender = 0;
		double descender = 0;
		int objects_traversed = 0;
		std::vector<LineSegment> segments;
	};

	struct CurrentLine
	{
		std::vector<SpanObject>::size_type object_index = 0;
		Line cur_line;
		double x_position = 0;
		double y_position = 0;
	};

	struct FloatBox
	{
		Rect rect;
		ObjectType type = object_image;
		std::shared_ptr<Image> image;
		Widget* component = nullptr;
		int id = -1;
	};

	TextSizeResult FindTextSize(Canvas* canvas, const TextBlock& block, size_t object_index);
	std::vector<TextBlock> FindTextBlocks();
	void LayoutLines(Canvas* canvas, double max_width);
	void LayoutText(Canvas* canvas, std::vector<TextBlock> blocks, std::vector<TextBlock>::size_type block_index, CurrentLine& current_line, double max_width);
	void LayoutBlock(CurrentLine& current_line, double max_width, std::vector<TextBlock>& blocks, std::vector<TextBlock>::size_type block_index);
	void LayoutFloatBlock(CurrentLine& current_line, double max_width);
	void LayoutInlineBlock(CurrentLine& current_line, double max_width, std::vector<TextBlock>& blocks, std::vector<TextBlock>::size_type block_index);
	void ReflowLine(CurrentLine& current_line, double max_width);
	FloatBox FloatBoxLeft(FloatBox float_box, double max_width);
	FloatBox FloatBoxRight(FloatBox float_box, double max_width);
	FloatBox FloatBoxAny(FloatBox box, double max_width, const std::vector<FloatBox>& floats1);
	bool BoxFitsOnLine(const FloatBox& box, double max_width);
	void PlaceLineSegments(CurrentLine& current_line, TextSizeResult& text_size_result);
	void ForcePlaceLineSegments(CurrentLine& current_line, TextSizeResult& text_size_result, double max_width);
	void NextLine(CurrentLine& current_line);
	bool IsNewline(const TextBlock& block);
	bool IsWhitespace(const TextBlock& block);
	bool FitsOnLine(double x_position, const TextSizeResult& text_size_result, double max_width);
	bool LargerThanLine(const TextSizeResult& text_size_result, double max_width);
	void AlignJustify(double max_width);
	void AlignCenter(double max_width);
	void AlignRight(double max_width);
	void DrawLayoutImage(Canvas* canvas, Line& line, LineSegment& segment, double x, double y);
	void DrawLayoutText(Canvas* canvas, Line& line, LineSegment& segment, double x, double y);

	bool cursor_visible = false;
	std::string::size_type cursor_pos = 0;
	bool cursor_overwrite_mode = false;
	Colorf cursor_color;

	std::string::size_type sel_start = 0, sel_end = 0;
	Colorf sel_foreground, sel_background = Colorf::fromRgba8(153, 201, 239);

	std::string text;
	std::vector<SpanObject> objects;
	std::vector<Line> lines;
	Point position;

	std::vector<FloatBox> floats_left, floats_right;

	SpanAlign alignment = span_left;

	struct LayoutCache
	{
		int object_index = -1;
		FontMetrics metrics;
	};
	LayoutCache layout_cache;

	bool is_ellipsis_draw = false;
	Rect ellipsis_content_rect;

	template<typename T>
	static T clamp(T val, T minval, T maxval) { return std::max<T>(std::min<T>(val, maxval), minval); }
};
