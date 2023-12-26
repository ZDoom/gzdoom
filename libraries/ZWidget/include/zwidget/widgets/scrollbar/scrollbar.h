
#pragma once

#include "../../core/widget.h"
#include "../../core/timer.h"
#include <functional>

class Scrollbar : public Widget
{
public:
	Scrollbar(Widget* parent);
	~Scrollbar();

	bool IsVertical() const;
	bool IsHorizontal() const;
	int GetMin() const;
	int GetMax() const;
	int GetLineStep() const;
	int GetPageStep() const;
	int GetPosition() const;

	void SetVertical();
	void SetHorizontal();

	void SetMin(int scroll_min);
	void SetMax(int scroll_max);
	void SetLineStep(int step);
	void SetPageStep(int step);

	void SetRanges(int scroll_min, int scroll_max, int line_step, int page_step);
	void SetRanges(int view_size, int total_size);

	void SetPosition(int pos);

	std::function<void()> FuncScroll;
	std::function<void()> FuncScrollMin;
	std::function<void()> FuncScrollMax;
	std::function<void()> FuncScrollLineDecrement;
	std::function<void()> FuncScrollLineIncrement;
	std::function<void()> FuncScrollPageDecrement;
	std::function<void()> FuncScrollPageIncrement;
	std::function<void()> FuncScrollThumbRelease;
	std::function<void()> FuncScrollThumbTrack;
	std::function<void()> FuncScrollEnd;

protected:
	void OnMouseMove(const Point& pos) override;
	void OnMouseDown(const Point& pos, int key) override;
	void OnMouseUp(const Point& pos, int key) override;
	void OnMouseLeave() override;
	void OnPaint(Canvas* canvas) override;
	void OnEnableChanged() override;
	void OnGeometryChanged() override;

private:
	bool UpdatePartPositions();
	int CalculateThumbSize(int track_size);
	int CalculateThumbPosition(int thumb_size, int track_size);
	Rect CreateRect(int start, int end);
	void InvokeScrollEvent(std::function<void()>* event_ptr);
	void OnTimerExpired();

	bool vertical = false;
	int scroll_min = 0;
	int scroll_max = 1;
	int line_step = 1;
	int page_step = 10;
	int position = 0;

	enum MouseDownMode
	{
		mouse_down_none,
		mouse_down_button_decr,
		mouse_down_button_incr,
		mouse_down_track_decr,
		mouse_down_track_incr,
		mouse_down_thumb_drag
	} mouse_down_mode = mouse_down_none;

	int thumb_start_position = 0;
	Point mouse_drag_start_pos;
	int thumb_start_pixel_position = 0;

	Timer* mouse_down_timer = nullptr;
	int last_step_size = 0;

	Rect rect_button_decrement;
	Rect rect_track_decrement;
	Rect rect_thumb;
	Rect rect_track_increment;
	Rect rect_button_increment;

	std::function<void()>* FuncScrollOnMouseDown = nullptr;

	static const int decr_height = 16;
	static const int incr_height = 16;
};
