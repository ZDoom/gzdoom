
#include "widgets/scrollbar/scrollbar.h"
#include "core/colorf.h"
#include <stdexcept>

Scrollbar::Scrollbar(Widget* parent) : Widget(parent)
{
	UpdatePartPositions();

	mouse_down_timer = new Timer(this);
	mouse_down_timer->FuncExpired = [=]() { OnTimerExpired(); };
}

Scrollbar::~Scrollbar()
{
}

bool Scrollbar::IsVertical() const
{
	return vertical;
}

bool Scrollbar::IsHorizontal() const
{
	return !vertical;
}

int Scrollbar::GetMin() const
{
	return scroll_min;
}

int Scrollbar::GetMax() const
{
	return scroll_max;
}

int Scrollbar::GetLineStep() const
{
	return line_step;
}

int Scrollbar::GetPageStep() const
{
	return page_step;
}

int Scrollbar::GetPosition() const
{
	return position;
}

void Scrollbar::SetVertical()
{
	vertical = true;
	if (UpdatePartPositions())
		Update();
}

void Scrollbar::SetHorizontal()
{
	vertical = false;
	if (UpdatePartPositions())
		Update();
}

void Scrollbar::SetMin(int new_scroll_min)
{
	SetRanges(new_scroll_min, scroll_max, line_step, page_step);
}

void Scrollbar::SetMax(int new_scroll_max)
{
	SetRanges(scroll_min, new_scroll_max, line_step, page_step);
}

void Scrollbar::SetLineStep(int step)
{
	SetRanges(scroll_min, scroll_max, step, page_step);
}

void Scrollbar::SetPageStep(int step)
{
	SetRanges(scroll_min, scroll_max, line_step, step);
}

void Scrollbar::SetRanges(int scroll_min, int scroll_max, int line_step, int page_step)
{
	if (scroll_min >= scroll_max || line_step <= 0 || page_step <= 0)
		throw std::runtime_error("Scrollbar ranges out of bounds!");
	scroll_min = scroll_min;
	scroll_max = scroll_max;
	line_step = line_step;
	page_step = page_step;
	if (position >= scroll_max)
		position = scroll_max - 1;
	if (position < scroll_min)
		position = scroll_min;
	if (UpdatePartPositions())
		Update();
}

void Scrollbar::SetRanges(int view_size, int total_size)
{
	if (view_size <= 0 || total_size <= 0)
	{
		SetRanges(0, 1, 1, 1);
	}
	else
	{
		int scroll_max = std::max(1, total_size - view_size + 1);
		int page_step = std::max(1, view_size);
		SetRanges(0, scroll_max, 1, page_step);
	}
}

void Scrollbar::SetPosition(int pos)
{
	position = pos;
	if (pos >= scroll_max)
		position = scroll_max - 1;
	if (pos < scroll_min)
		position = scroll_min;

	if (UpdatePartPositions())
		Update();
}

void Scrollbar::OnMouseMove(const Point& pos)
{
	if (mouse_down_mode == mouse_down_thumb_drag)
	{
		int last_position = position;

		if (pos.x < -100 || pos.x > GetWidth() + 100 || pos.y < -100 || pos.y > GetHeight() + 100)
		{
			position = thumb_start_position;
		}
		else
		{
			int delta = (int)(vertical ? (pos.y - mouse_drag_start_pos.y) : (pos.x - mouse_drag_start_pos.x));
			int position_pixels = thumb_start_pixel_position + delta;

			int track_height = 0;
			if (vertical)
				track_height = (int)(rect_track_decrement.height + rect_track_increment.height);
			else
				track_height = (int)(rect_track_decrement.width + rect_track_increment.width);

			if (track_height != 0)
				position = scroll_min + position_pixels * (scroll_max - scroll_min) / track_height;
			else
				position = 0;

			if (position >= scroll_max)
				position = scroll_max - 1;
			if (position < scroll_min)
				position = scroll_min;

		}

		if (position != last_position)
		{
			InvokeScrollEvent(&FuncScrollThumbTrack);
			UpdatePartPositions();
		}
	}

	Update();
}

void Scrollbar::OnMouseDown(const Point& pos, int key)
{
	mouse_drag_start_pos = pos;

	if (rect_button_decrement.contains(pos))
	{
		mouse_down_mode = mouse_down_button_decr;
		FuncScrollOnMouseDown = &FuncScrollLineDecrement;

		int last_position = position;

		position -= line_step;
		last_step_size = -line_step;
		if (position >= scroll_max)
			position = scroll_max - 1;
		if (position < scroll_min)
			position = scroll_min;

		if (last_position != position)
			InvokeScrollEvent(&FuncScrollLineDecrement);
	}
	else if (rect_button_increment.contains(pos))
	{
		mouse_down_mode = mouse_down_button_incr;
		FuncScrollOnMouseDown = &FuncScrollLineIncrement;

		int last_position = position;

		position += line_step;
		last_step_size = line_step;
		if (position >= scroll_max)
			position = scroll_max - 1;
		if (position < scroll_min)
			position = scroll_min;

		if (last_position != position)
			InvokeScrollEvent(&FuncScrollLineIncrement);
	}
	else if (rect_thumb.contains(pos))
	{
		mouse_down_mode = mouse_down_thumb_drag;
		thumb_start_position = position;
		thumb_start_pixel_position = (int)(vertical ? (rect_thumb.y - rect_track_decrement.y) : (rect_thumb.x - rect_track_decrement.x));
	}
	else if (rect_track_decrement.contains(pos))
	{
		mouse_down_mode = mouse_down_track_decr;
		FuncScrollOnMouseDown = &FuncScrollPageDecrement;

		int last_position = position;

		position -= page_step;
		last_step_size = -page_step;
		if (position >= scroll_max)
			position = scroll_max - 1;
		if (position < scroll_min)
			position = scroll_min;

		if (last_position != position)
			InvokeScrollEvent(&FuncScrollPageDecrement);
	}
	else if (rect_track_increment.contains(pos))
	{
		mouse_down_mode = mouse_down_track_incr;
		FuncScrollOnMouseDown = &FuncScrollPageIncrement;

		int last_position = position;

		position += page_step;
		last_step_size = page_step;
		if (position >= scroll_max)
			position = scroll_max - 1;
		if (position < scroll_min)
			position = scroll_min;

		if (last_position != position)
			InvokeScrollEvent(&FuncScrollPageIncrement);
	}

	mouse_down_timer->Start(100, false);

	UpdatePartPositions();

	Update();
	CaptureMouse();
}

void Scrollbar::OnMouseUp(const Point& pos, int key)
{
	if (mouse_down_mode == mouse_down_thumb_drag)
	{
		if (FuncScrollThumbRelease)
			FuncScrollThumbRelease();
	}

	mouse_down_mode = mouse_down_none;
	mouse_down_timer->Stop();

	Update();
	ReleaseMouseCapture();
}

void Scrollbar::OnMouseLeave()
{
	Update();
}

void Scrollbar::OnGeometryChanged()
{
	UpdatePartPositions();
}

void Scrollbar::OnPaint(Canvas* canvas)
{
	/*
	part_button_decrement.render_box(canvas, rect_button_decrement);
	part_track_decrement.render_box(canvas, rect_track_decrement);
	part_thumb.render_box(canvas, rect_thumb);
	part_thumb_gripper.render_box(canvas, rect_thumb);
	part_track_increment.render_box(canvas, rect_track_increment);
	part_button_increment.render_box(canvas, rect_button_increment);
	*/
}

// Calculates positions of all parts. Returns true if thumb position was changed compared to previously, false otherwise.
bool Scrollbar::UpdatePartPositions()
{
	int total_height = (int)(vertical ? GetHeight() : GetWidth());
	int track_height = std::max(0, total_height - decr_height - incr_height);
	int thumb_height = CalculateThumbSize(track_height);

	int thumb_offset = decr_height + CalculateThumbPosition(thumb_height, track_height);

	Rect previous_rect_thumb = rect_thumb;

	rect_button_decrement = CreateRect(0, decr_height);
	rect_track_decrement = CreateRect(decr_height, thumb_offset);
	rect_thumb = CreateRect(thumb_offset, thumb_offset + thumb_height);
	rect_track_increment = CreateRect(thumb_offset + thumb_height, decr_height + track_height);
	rect_button_increment = CreateRect(decr_height + track_height, decr_height + track_height + incr_height);

	return (previous_rect_thumb != rect_thumb);
}

int Scrollbar::CalculateThumbSize(int track_size)
{
	int minimum_thumb_size = 20;
	int range = scroll_max - scroll_min;
	int length = range + page_step - 1;
	int thumb_size = page_step * track_size / length;
	if (thumb_size < minimum_thumb_size)
		thumb_size = minimum_thumb_size;
	if (thumb_size > track_size)
		thumb_size = track_size;
	return thumb_size;
}

int Scrollbar::CalculateThumbPosition(int thumb_size, int track_size)
{
	int relative_pos = position - scroll_min;
	int range = scroll_max - scroll_min - 1;
	if (range != 0)
	{
		int available_area = std::max(0, track_size - thumb_size);
		return relative_pos * available_area / range;
	}
	else
	{
		return 0;
	}
}

Rect Scrollbar::CreateRect(int start, int end)
{
	if (vertical)
		return Rect(0.0, start, GetWidth(), end - start);
	else
		return Rect(start, 0.0, end - start, GetHeight());
}

void Scrollbar::OnTimerExpired()
{
	if (mouse_down_mode == mouse_down_thumb_drag)
		return;

	mouse_down_timer->Start(100, false);

	int last_position = position;
	position += last_step_size;
	if (position >= scroll_max)
		position = scroll_max - 1;

	if (position < scroll_min)
		position = scroll_min;

	if (position != last_position)
	{
		InvokeScrollEvent(FuncScrollOnMouseDown);

		if (UpdatePartPositions())
			Update();
	}
}

void Scrollbar::OnEnableChanged()
{
	Update();
}

void Scrollbar::InvokeScrollEvent(std::function<void()>* event_ptr)
{
	if (position == scroll_max - 1)
	{
		if (FuncScrollMax)
			FuncScrollMax();
	}

	if (position == scroll_min)
	{
		if (FuncScrollMin)
			FuncScrollMin();
	}

	if (FuncScroll)
		FuncScroll();

	if (event_ptr)
		(*event_ptr)();
}
