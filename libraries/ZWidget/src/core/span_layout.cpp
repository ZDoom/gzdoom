
#include "core/span_layout.h"
#include "core/canvas.h"
#include "core/widget.h"
#include "core/font.h"
#include "core/image.h"

SpanLayout::SpanLayout()
{
}

SpanLayout::~SpanLayout()
{
}

void SpanLayout::Clear()
{
	objects.clear();
	text.clear();
	lines.clear();
}

std::vector<Rect> SpanLayout::GetRectById(int id) const
{
	std::vector<Rect> segment_rects;

	double x = position.x;
	double y = position.y;
	for (std::vector<Line>::size_type line_index = 0; line_index < lines.size(); line_index++)
	{
		const Line& line = lines[line_index];
		for (std::vector<LineSegment>::size_type segment_index = 0; segment_index < line.segments.size(); segment_index++)
		{
			const LineSegment& segment = line.segments[segment_index];
			if (segment.id == id)
			{
				segment_rects.push_back(Rect(x + segment.x_position, y, segment.width, y + line.height));
			}
		}
		y += line.height;
	}

	return segment_rects;
}

void SpanLayout::DrawLayout(Canvas* canvas)
{
	double x = position.x;
	double y = position.y;
	for (std::vector<Line>::size_type line_index = 0; line_index < lines.size(); line_index++)
	{
		Line& line = lines[line_index];
		for (std::vector<LineSegment>::size_type segment_index = 0; segment_index < line.segments.size(); segment_index++)
		{
			LineSegment& segment = line.segments[segment_index];
			switch (segment.type)
			{
			case object_text:
				DrawLayoutText(canvas, line, segment, x, y);
				break;
			case object_image:
				DrawLayoutImage(canvas, line, segment, x, y);
				break;
			case object_component:
				break;
			}

		}

		if (line_index + 1 == lines.size() && !line.segments.empty())
		{
			LineSegment& segment = line.segments.back();
			if (cursor_visible && segment.end <= cursor_pos)
			{
				switch (segment.type)
				{
				case object_text:
				{
					double cursor_x = x + segment.x_position + canvas->measureText(segment.font, text.substr(segment.start, segment.end - segment.start)).width;
					double cursor_width = 1;
					canvas->fillRect(Rect::ltrb(cursor_x, y + line.ascender - segment.ascender, cursor_width, y + line.ascender + segment.descender), cursor_color);
				}
				break;
				}
			}
		}

		y += line.height;
	}
}

void SpanLayout::DrawLayoutEllipsis(Canvas* canvas, const Rect& content_rect)
{
	is_ellipsis_draw = true;
	ellipsis_content_rect = content_rect;
	try
	{
		is_ellipsis_draw = false;
		DrawLayout(canvas);
	}
	catch (...)
	{
		is_ellipsis_draw = false;
		throw;
	}
}

void SpanLayout::DrawLayoutImage(Canvas* canvas, Line& line, LineSegment& segment, double x, double y)
{
	canvas->drawImage(segment.image, Point(x + segment.x_position, y + line.ascender - segment.ascender));
}

void SpanLayout::DrawLayoutText(Canvas* canvas, Line& line, LineSegment& segment, double x, double y)
{
	std::string segment_text = text.substr(segment.start, segment.end - segment.start);

	int length = (int)segment_text.length();
	int s1 = clamp((int)sel_start - (int)segment.start, 0, length);
	int s2 = clamp((int)sel_end - (int)segment.start, 0, length);

	if (s1 != s2)
	{
		double xx = x + segment.x_position;
		double xx0 = xx + canvas->measureText(segment.font, segment_text.substr(0, s1)).width;
		double xx1 = xx0 + canvas->measureText(segment.font, segment_text.substr(s1, s2 - s1)).width;
		double sel_width = canvas->measureText(segment.font, segment_text.substr(s1, s2 - s1)).width;

		canvas->fillRect(Rect::ltrb(xx0, y + line.ascender - segment.ascender, xx1, y + line.ascender + segment.descender), sel_background);

		if (cursor_visible && cursor_pos >= segment.start && cursor_pos < segment.end)
		{
			double cursor_x = x + segment.x_position + canvas->measureText(segment.font, text.substr(segment.start, cursor_pos - segment.start)).width;
			double cursor_width = cursor_overwrite_mode ? canvas->measureText(segment.font, text.substr(cursor_pos, 1)).width : 1;
			canvas->fillRect(Rect::ltrb(cursor_x, y + line.ascender - segment.ascender, cursor_x + cursor_width, y + line.ascender + segment.descender), cursor_color);
		}

		if (s1 > 0)
		{
			if (is_ellipsis_draw)
				canvas->drawTextEllipsis(segment.font, Point(xx, y + line.ascender), ellipsis_content_rect, segment_text.substr(0, s1), segment.color);
			else
				canvas->drawText(segment.font, Point(xx, y + line.ascender), segment_text.substr(0, s1), segment.color);
		}
		if (is_ellipsis_draw)
			canvas->drawTextEllipsis(segment.font, Point(xx0, y + line.ascender), ellipsis_content_rect, segment_text.substr(s1, s2 - s1), sel_foreground);
		else
			canvas->drawText(segment.font, Point(xx0, y + line.ascender), segment_text.substr(s1, s2 - s1), sel_foreground);
		xx += sel_width;
		if (s2 < length)
		{
			if (is_ellipsis_draw)
				canvas->drawTextEllipsis(segment.font, Point(xx1, y + line.ascender), ellipsis_content_rect, segment_text.substr(s2), segment.color);
			else
				canvas->drawText(segment.font, Point(xx1, y + line.ascender), segment_text.substr(s2), segment.color);
		}
	}
	else
	{
		if (cursor_visible && cursor_pos >= segment.start && cursor_pos < segment.end)
		{
			double cursor_x = x + segment.x_position + canvas->measureText(segment.font, text.substr(segment.start, cursor_pos - segment.start)).width;
			double cursor_width = cursor_overwrite_mode ? canvas->measureText(segment.font, text.substr(cursor_pos, 1)).width : 1;
			canvas->fillRect(Rect::ltrb(cursor_x, y + line.ascender - segment.ascender, cursor_x + cursor_width, y + line.ascender + segment.descender), cursor_color);
		}

		if (is_ellipsis_draw)
			canvas->drawTextEllipsis(segment.font, Point(x + segment.x_position, y + line.ascender), ellipsis_content_rect, segment_text, segment.color);
		else
			canvas->drawText(segment.font, Point(x + segment.x_position, y + line.ascender), segment_text, segment.color);
	}
}

SpanLayout::HitTestResult SpanLayout::HitTest(Canvas* canvas, const Point& pos)
{
	SpanLayout::HitTestResult result;

	if (lines.empty())
	{
		result.type = SpanLayout::HitTestResult::no_objects_available;
		return result;
	}

	double x = position.x;
	double y = position.y;

	// Check if we are outside to the top
	if (pos.y < y)
	{
		result.type = SpanLayout::HitTestResult::outside_top;
		result.object_id = lines[0].segments[0].id;
		result.offset = 0;
		return result;
	}

	for (std::vector<Line>::size_type line_index = 0; line_index < lines.size(); line_index++)
	{
		Line& line = lines[line_index];

		// Check if we found current line
		if (pos.y >= y && pos.y <= y + line.height)
		{
			for (std::vector<LineSegment>::size_type segment_index = 0; segment_index < line.segments.size(); segment_index++)
			{
				LineSegment& segment = line.segments[segment_index];

				// Check if we are outside to the left
				if (segment_index == 0 && pos.x < x)
				{
					result.type = SpanLayout::HitTestResult::outside_left;
					result.object_id = segment.id;
					result.offset = segment.start;
					return result;
				}

				// Check if we are inside a segment
				if (pos.x >= x + segment.x_position && pos.x <= x + segment.x_position + segment.width)
				{
					std::string segment_text = text.substr(segment.start, segment.end - segment.start);
					Point hit_point(pos.x - x - segment.x_position, 0);
					size_t offset = segment.start + canvas->getCharacterIndex(segment.font, segment_text, hit_point);

					result.type = SpanLayout::HitTestResult::inside;
					result.object_id = segment.id;
					result.offset = offset;
					return result;
				}

				// Check if we are outside to the right
				if (segment_index == line.segments.size() - 1 && pos.x > x + segment.x_position + segment.width)
				{
					result.type = SpanLayout::HitTestResult::outside_right;
					result.object_id = segment.id;
					result.offset = segment.end;
					return result;
				}
			}
		}

		y += line.height;
	}

	// We are outside to the bottom
	const Line& last_line = lines[lines.size() - 1];
	const LineSegment& last_segment = last_line.segments[last_line.segments.size() - 1];

	result.type = SpanLayout::HitTestResult::outside_bottom;
	result.object_id = last_segment.id;
	result.offset = last_segment.end;
	return result;
}

Size SpanLayout::GetSize() const
{
	return GetRect().size();
}

Rect SpanLayout::GetRect() const
{
	double x = position.x;
	double y = position.y;

	const double max_value = 0x70000000;
	double left = max_value;
	double top = max_value;
	double right = -max_value;
	double bottom = -max_value;

	for (std::vector<Line>::size_type line_index = 0; line_index < lines.size(); line_index++)
	{
		const Line& line = lines[line_index];
		for (std::vector<LineSegment>::size_type segment_index = 0; segment_index < line.segments.size(); segment_index++)
		{
			const LineSegment& segment = line.segments[segment_index];
			Rect area(Point(x + segment.x_position, y), Size(segment.width, line.height));

			left = std::min(left, area.left());
			right = std::max(right, area.right());
			top = std::min(top, area.top());
			bottom = std::max(bottom, area.bottom());
		}
		y += line.height;
	}
	if (left > right)
		left = right = position.x;

	if (top > bottom)
		top = bottom = position.y;

	return Rect::ltrb(left, top, right, bottom);
}

void SpanLayout::AddText(const std::string& more_text, std::shared_ptr<Font> font, const Colorf& color, int id)
{
	SpanObject object;
	object.type = object_text;
	object.start = text.length();
	object.end = object.start + more_text.length();
	object.font = font;
	object.color = color;
	object.id = id;
	objects.push_back(object);
	text += more_text;
}

void SpanLayout::AddImage(std::shared_ptr<Image> image, double baseline_offset, int id)
{
	SpanObject object;
	object.type = object_image;
	object.image = image;
	object.baseline_offset = baseline_offset;
	object.id = id;
	object.start = text.length();
	object.end = object.start + 1;
	objects.push_back(object);
	text += "*";
}

void SpanLayout::AddWidget(Widget* component, double baseline_offset, int id)
{
	SpanObject object;
	object.type = object_component;
	object.component = component;
	object.baseline_offset = baseline_offset;
	object.id = id;
	object.start = text.length();
	object.end = object.start + 1;
	objects.push_back(object);
	text += "*";
}

void SpanLayout::Layout(Canvas* canvas, double max_width)
{
	LayoutLines(canvas, max_width);

	switch (alignment)
	{
	case span_right: AlignRight(max_width); break;
	case span_center: AlignCenter(max_width); break;
	case span_justify: AlignJustify(max_width); break;
	case span_left:
	default: break;
	}
}

void SpanLayout::SetPosition(const Point& pos)
{
	position = pos;
}

SpanLayout::TextSizeResult SpanLayout::FindTextSize(Canvas* canvas, const TextBlock& block, size_t object_index)
{
	std::shared_ptr<Font> font = objects[object_index].font;
	if (layout_cache.object_index != (int)object_index)
	{
		layout_cache.object_index = (int)object_index;
		layout_cache.metrics = canvas->getFontMetrics(font);
	}

	TextSizeResult result;
	result.start = block.start;
	size_t pos = block.start;
	double x_position = 0;
	while (pos != block.end)
	{
		size_t end = std::min(objects[object_index].end, block.end);
		std::string subtext = text.substr(pos, end - pos);

		Size text_size = canvas->measureText(font, subtext).size();

		result.width += text_size.width;
		result.height = std::max(result.height, layout_cache.metrics.height + layout_cache.metrics.external_leading);
		result.ascender = std::max(result.ascender, layout_cache.metrics.ascent);
		result.descender = std::max(result.descender, layout_cache.metrics.descent);

		LineSegment segment;
		segment.type = object_text;
		segment.start = pos;
		segment.end = end;
		segment.font = objects[object_index].font;
		segment.color = objects[object_index].color;
		segment.id = objects[object_index].id;
		segment.x_position = x_position;
		segment.width = text_size.width;
		segment.ascender = layout_cache.metrics.ascent;
		segment.descender = layout_cache.metrics.descent;
		x_position += text_size.width;
		result.segments.push_back(segment);

		pos = end;
		if (pos == objects[object_index].end)
		{
			object_index++;
			result.objects_traversed++;

			if (object_index < objects.size())
			{
				layout_cache.object_index = (int)object_index;
				font = objects[object_index].font;
				layout_cache.metrics = canvas->getFontMetrics(font);
			}
		}
	}
	result.end = pos;
	return result;
}

std::vector<SpanLayout::TextBlock> SpanLayout::FindTextBlocks()
{
	std::vector<TextBlock> blocks;
	std::vector<SpanObject>::iterator block_object_it;

	// Find first object that is not text:
	for (block_object_it = objects.begin(); block_object_it != objects.end() && (*block_object_it).type == object_text; ++block_object_it);

	std::string::size_type pos = 0;
	while (pos < text.size())
	{
		// Find end of text block:
		std::string::size_type end_pos;
		switch (text[pos])
		{
		case ' ':
		case '\t':
		case '\n':
			end_pos = text.find_first_not_of(text[pos], pos);
			break;
		default:
			end_pos = text.find_first_of(" \t\n", pos);
			break;
		}

		if (end_pos == std::string::npos)
			end_pos = text.length();

		// If we traversed past an object that is not text:
		if (block_object_it != objects.end() && (*block_object_it).start < end_pos)
		{
			// End text block
			end_pos = (*block_object_it).start;
			if (end_pos > pos)
			{
				TextBlock block;
				block.start = pos;
				block.end = end_pos;
				blocks.push_back(block);
			}

			// Create object block:
			pos = end_pos;
			end_pos = pos + 1;

			TextBlock block;
			block.start = pos;
			block.end = end_pos;
			blocks.push_back(block);

			// Find next object that is not text:
			for (++block_object_it; block_object_it != objects.end() && (*block_object_it).type == object_text; ++block_object_it);
		}
		else
		{
			if (end_pos > pos)
			{
				TextBlock block;
				block.start = pos;
				block.end = end_pos;
				blocks.push_back(block);
			}
		}

		pos = end_pos;
	}

	return blocks;
}

void SpanLayout::SetAlign(SpanAlign align)
{
	alignment = align;
}

void SpanLayout::LayoutLines(Canvas* canvas, double max_width)
{
	lines.clear();
	if (objects.empty())
		return;

	layout_cache.metrics = {};
	layout_cache.object_index = -1;

	CurrentLine current_line;
	std::vector<TextBlock> blocks = FindTextBlocks();
	for (std::vector<TextBlock>::size_type block_index = 0; block_index < blocks.size(); block_index++)
	{
		if (objects[current_line.object_index].type == object_text)
			LayoutText(canvas, blocks, block_index, current_line, max_width);
		else
			LayoutBlock(current_line, max_width, blocks, block_index);
	}
	NextLine(current_line);
}

void SpanLayout::LayoutBlock(CurrentLine& current_line, double max_width, std::vector<TextBlock>& blocks, std::vector<TextBlock>::size_type block_index)
{
	if (objects[current_line.object_index].float_type == float_none)
		LayoutInlineBlock(current_line, max_width, blocks, block_index);
	else
		LayoutFloatBlock(current_line, max_width);

	current_line.object_index++;
}

void SpanLayout::LayoutInlineBlock(CurrentLine& current_line, double max_width, std::vector<TextBlock>& blocks, std::vector<TextBlock>::size_type block_index)
{
	Size size;
	LineSegment segment;
	if (objects[current_line.object_index].type == object_image)
	{
		size = Size(objects[current_line.object_index].image->GetWidth(), objects[current_line.object_index].image->GetHeight());
		segment.type = object_image;
		segment.image = objects[current_line.object_index].image;
	}
	else if (objects[current_line.object_index].type == object_component)
	{
		size = objects[current_line.object_index].component->GetSize();
		segment.type = object_component;
		segment.component = objects[current_line.object_index].component;
	}

	if (current_line.x_position + size.width > max_width)
		NextLine(current_line);

	segment.x_position = current_line.x_position;
	segment.width = size.width;
	segment.start = blocks[block_index].start;
	segment.end = blocks[block_index].end;
	segment.id = objects[current_line.object_index].id;
	segment.ascender = size.height - objects[current_line.object_index].baseline_offset;
	current_line.cur_line.segments.push_back(segment);
	current_line.cur_line.height = std::max(current_line.cur_line.height, size.height + objects[current_line.object_index].baseline_offset);
	current_line.cur_line.ascender = std::max(current_line.cur_line.ascender, segment.ascender);
	current_line.x_position += size.width;
}

void SpanLayout::LayoutFloatBlock(CurrentLine& current_line, double max_width)
{
	FloatBox floatbox;
	floatbox.type = objects[current_line.object_index].type;
	floatbox.image = objects[current_line.object_index].image;
	floatbox.component = objects[current_line.object_index].component;
	floatbox.id = objects[current_line.object_index].id;
	if (objects[current_line.object_index].type == object_image)
		floatbox.rect = Rect::xywh(0, current_line.y_position, floatbox.image->GetWidth(), floatbox.image->GetHeight());
	else if (objects[current_line.object_index].type == object_component)
		floatbox.rect = Rect::xywh(0, current_line.y_position, floatbox.component->GetWidth(), floatbox.component->GetHeight());

	if (objects[current_line.object_index].float_type == float_left)
		floats_left.push_back(FloatBoxLeft(floatbox, max_width));
	else
		floats_right.push_back(FloatBoxRight(floatbox, max_width));

	ReflowLine(current_line, max_width);
}

void SpanLayout::ReflowLine(CurrentLine& step, double max_width)
{
}

SpanLayout::FloatBox SpanLayout::FloatBoxLeft(FloatBox box, double max_width)
{
	return FloatBoxAny(box, max_width, floats_left);
}

SpanLayout::FloatBox SpanLayout::FloatBoxRight(FloatBox box, double max_width)
{
	return FloatBoxAny(box, max_width, floats_right);
}

SpanLayout::FloatBox SpanLayout::FloatBoxAny(FloatBox box, double max_width, const std::vector<FloatBox>& floats1)
{
	bool restart;
	do
	{
		restart = false;
		for (size_t i = 0; i < floats1.size(); i++)
		{
			double top = std::max(floats1[i].rect.top(), box.rect.top());
			double bottom = std::min(floats1[i].rect.bottom(), box.rect.bottom());
			if (bottom > top && box.rect.left() < floats1[i].rect.right())
			{
				Size s = box.rect.size();
				box.rect.x = floats1[i].rect.x;
				box.rect.width = s.width;

				if (!BoxFitsOnLine(box, max_width))
				{
					box.rect.x = 0;
					box.rect.width = s.width;
					box.rect.y = floats1[i].rect.bottom();
					box.rect.height = s.height;
					restart = true;
					break;
				}
			}
		}
	} while (restart);
	return box;
}

bool SpanLayout::BoxFitsOnLine(const FloatBox& box, double max_width)
{
	for (size_t i = 0; i < floats_right.size(); i++)
	{
		double top = std::max(floats_right[i].rect.top(), box.rect.top());
		double bottom = std::min(floats_right[i].rect.bottom(), box.rect.bottom());
		if (bottom > top)
		{
			if (box.rect.right() + floats_right[i].rect.right() > max_width)
				return false;
		}
	}
	return true;
}

void SpanLayout::LayoutText(Canvas* canvas, std::vector<TextBlock> blocks, std::vector<TextBlock>::size_type block_index, CurrentLine& current_line, double max_width)
{
	TextSizeResult text_size_result = FindTextSize(canvas, blocks[block_index], current_line.object_index);
	current_line.object_index += text_size_result.objects_traversed;

	current_line.cur_line.width = current_line.x_position;

	if (IsNewline(blocks[block_index]))
	{
		current_line.cur_line.height = std::max(current_line.cur_line.height, text_size_result.height);
		current_line.cur_line.ascender = std::max(current_line.cur_line.ascender, text_size_result.ascender);
		NextLine(current_line);
	}
	else
	{
		if (!FitsOnLine(current_line.x_position, text_size_result, max_width) && !IsWhitespace(blocks[block_index]))
		{
			if (LargerThanLine(text_size_result, max_width))
			{
				// force line breaks to make it fit
				ForcePlaceLineSegments(current_line, text_size_result, max_width);
			}
			else
			{
				NextLine(current_line);
				PlaceLineSegments(current_line, text_size_result);
			}
		}
		else
		{
			PlaceLineSegments(current_line, text_size_result);
		}
	}
}

void SpanLayout::NextLine(CurrentLine& current_line)
{
	current_line.cur_line.width = current_line.x_position;
	for (std::vector<LineSegment>::reverse_iterator it = current_line.cur_line.segments.rbegin(); it != current_line.cur_line.segments.rend(); ++it)
	{
		LineSegment& segment = *it;
		if (segment.type == object_text)
		{
			std::string s = text.substr(segment.start, segment.end - segment.start);
			if (s.find_first_not_of(" \t\r\n") != std::string::npos)
			{
				current_line.cur_line.width = segment.x_position + segment.width;
				break;
			}
			else
			{
				// We remove the width so that GetRect() reports the correct sizes
				segment.width = 0;
			}
		}
		else
		{
			current_line.cur_line.width = segment.x_position + segment.width;
			break;
		}
	}

	double height = current_line.cur_line.height;
	lines.push_back(current_line.cur_line);
	current_line.cur_line = Line();
	current_line.x_position = 0;
	current_line.y_position += height;
}

void SpanLayout::PlaceLineSegments(CurrentLine& current_line, TextSizeResult& text_size_result)
{
	for (std::vector<LineSegment>::iterator it = text_size_result.segments.begin(); it != text_size_result.segments.end(); ++it)
	{
		LineSegment segment = *it;
		segment.x_position += current_line.x_position;
		current_line.cur_line.segments.push_back(segment);
	}
	current_line.x_position += text_size_result.width;
	current_line.cur_line.height = std::max(current_line.cur_line.height, text_size_result.height);
	current_line.cur_line.ascender = std::max(current_line.cur_line.ascender, text_size_result.ascender);
}

void SpanLayout::ForcePlaceLineSegments(CurrentLine& current_line, TextSizeResult& text_size_result, double max_width)
{
	if (current_line.x_position != 0)
		NextLine(current_line);

	// to do: do this properly - for now we just place the entire block on one line
	PlaceLineSegments(current_line, text_size_result);
}

bool SpanLayout::IsNewline(const TextBlock& block)
{
	return block.start != block.end && text[block.start] == '\n';
}

bool SpanLayout::IsWhitespace(const TextBlock& block)
{
	return block.start != block.end && text[block.start] == ' ';
}

bool SpanLayout::FitsOnLine(double x_position, const TextSizeResult& text_size_result, double max_width)
{
	return x_position + text_size_result.width <= max_width;
}

bool SpanLayout::LargerThanLine(const TextSizeResult& text_size_result, double max_width)
{
	return text_size_result.width > max_width;
}

void SpanLayout::AlignRight(double max_width)
{
	for (std::vector<Line>::size_type line_index = 0; line_index < lines.size(); line_index++)
	{
		Line& line = lines[line_index];
		double offset = max_width - line.width;
		if (offset < 0) offset = 0;

		for (std::vector<LineSegment>::size_type segment_index = 0; segment_index < line.segments.size(); segment_index++)
		{
			LineSegment& segment = line.segments[segment_index];
			segment.x_position += offset;
		}
	}
}

void SpanLayout::AlignCenter(double max_width)
{
	for (std::vector<Line>::size_type line_index = 0; line_index < lines.size(); line_index++)
	{
		Line& line = lines[line_index];
		double offset = (max_width - line.width) / 2;
		if (offset < 0) offset = 0;

		for (std::vector<LineSegment>::size_type segment_index = 0; segment_index < line.segments.size(); segment_index++)
		{
			LineSegment& segment = line.segments[segment_index];
			segment.x_position += offset;
		}
	}
}

void SpanLayout::AlignJustify(double max_width)
{
	// Note, we do not justify the last line
	for (std::vector<Line>::size_type line_index = 0; line_index + 1 < lines.size(); line_index++)
	{
		Line& line = lines[line_index];
		double offset = max_width - line.width;
		if (offset < 0) offset = 0;

		if (line.segments.size() <= 1)	// Do not justify line if only one word exists
			continue;

		for (std::vector<LineSegment>::size_type segment_index = 0; segment_index < line.segments.size(); segment_index++)
		{
			LineSegment& segment = line.segments[segment_index];
			segment.x_position += (offset * segment_index) / (line.segments.size() - 1);
		}
	}
}

Size SpanLayout::FindPreferredSize(Canvas* canvas)
{
	LayoutLines(canvas, 0x70000000); // Feed it with a very long length so it ends up on one line
	return GetRect().size();
}

void SpanLayout::SetSelectionRange(std::string::size_type start, std::string::size_type end)
{
	sel_start = start;
	sel_end = end;
	if (sel_end < sel_start)
		sel_end = sel_start;
}

void SpanLayout::SetSelectionColors(const Colorf& foreground, const Colorf& background)
{
	sel_foreground = foreground;
	sel_background = background;
}

void SpanLayout::ShowCursor()
{
	cursor_visible = true;
}

void SpanLayout::HideCursor()
{
	cursor_visible = false;
}

void SpanLayout::SetCursorPos(std::string::size_type pos)
{
	cursor_pos = pos;
}

void SpanLayout::SetCursorOverwriteMode(bool enable)
{
	cursor_overwrite_mode = enable;
}

void SpanLayout::SetCursorColor(const Colorf& color)
{
	cursor_color = color;
}

std::string SpanLayout::GetCombinedText() const
{
	return text;
}

void SpanLayout::SetComponentGeometry()
{
	double x = position.x;
	double y = position.y;
	for (size_t i = 0; i < lines.size(); i++)
	{
		for (size_t j = 0; j < lines[i].segments.size(); j++)
		{
			if (lines[i].segments[j].type == object_component)
			{
				Point pos(x + lines[i].segments[j].x_position, y + lines[i].ascender - lines[i].segments[j].ascender);
				Size size = lines[i].segments[j].component->GetSize();
				Rect rect(pos, size);
				lines[i].segments[j].component->SetFrameGeometry(rect);
			}
		}
		y += lines[i].height;
	}
}

double SpanLayout::GetFirstBaselineOffset()
{
	if (!lines.empty())
	{
		return lines.front().ascender;
	}
	else
	{
		return 0;
	}
}

double SpanLayout::GetLastBaselineOffset()
{
	if (!lines.empty())
	{
		double y = 0;
		for (size_t i = 0; i + 1 < lines.size(); i++)
			y += lines[i].height;
		return y + lines.back().ascender;
	}
	else
	{
		return 0;
	}
}
