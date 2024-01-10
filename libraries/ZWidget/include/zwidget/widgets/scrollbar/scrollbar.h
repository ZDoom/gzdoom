
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
	double GetMin() const;
	double GetMax() const;
	double GetLineStep() const;
	double GetPageStep() const;
	double GetPosition() const;

	void SetVertical();
	void SetHorizontal();

	void SetMin(double scroll_min);
	void SetMax(double scroll_max);
	void SetLineStep(double step);
	void SetPageStep(double step);

	void SetRanges(double scroll_min, double scroll_max, double line_step, double page_step);
	void SetRanges(double view_size, double total_size);

	void SetPosition(double pos);

	double GetPreferredWidth() const { return 16.0; }
	double GetPreferredHeight() const { return 16.0; }

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
	bool OnMouseDown(const Point& pos, int key) override;
	bool OnMouseUp(const Point& pos, int key) override;
	void OnMouseMove(const Point& pos) override;
	void OnMouseLeave() override;
	void OnPaint(Canvas* canvas) override;
	void OnEnableChanged() override;
	void OnGeometryChanged() override;

private:
	bool UpdatePartPositions();
	double CalculateThumbSize(double track_size);
	double CalculateThumbPosition(double thumb_size, double track_size);
	Rect CreateRect(double start, double end);
	void InvokeScrollEvent(std::function<void()>* event_ptr);
	void OnTimerExpired();

	bool vertical = true;
	double scroll_min = 0.0;
	double scroll_max = 1.0;
	double line_step = 1.0;
	double page_step = 10.0;
	double position = 0.0;

	bool showbuttons = false;

	enum MouseDownMode
	{
		mouse_down_none,
		mouse_down_button_decr,
		mouse_down_button_incr,
		mouse_down_track_decr,
		mouse_down_track_incr,
		mouse_down_thumb_drag
	} mouse_down_mode = mouse_down_none;

	double thumb_start_position = 0.0;
	Point mouse_drag_start_pos;
	double thumb_start_pixel_position = 0.0;

	Timer* mouse_down_timer = nullptr;
	double last_step_size = 0.0;

	Rect rect_button_decrement;
	Rect rect_track_decrement;
	Rect rect_thumb;
	Rect rect_track_increment;
	Rect rect_button_increment;

	std::function<void()>* FuncScrollOnMouseDown = nullptr;
};
