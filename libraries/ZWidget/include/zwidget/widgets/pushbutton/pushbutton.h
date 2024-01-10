
#pragma once

#include "../../core/widget.h"
#include <functional>

class PushButton : public Widget
{
public:
	PushButton(Widget* parent = nullptr);

	void SetText(const std::string& value);
	const std::string& GetText() const;

	double GetPreferredHeight() const;

	void Click();

	std::function<void()> OnClick;

protected:
	void OnPaintFrame(Canvas* canvas) override;
	void OnPaint(Canvas* canvas) override;
	bool OnMouseDown(const Point& pos, int key) override;
	bool OnMouseUp(const Point& pos, int key) override;
	void OnMouseMove(const Point& pos) override;
	void OnMouseLeave() override;
	void OnKeyDown(EInputKey key) override;
	void OnKeyUp(EInputKey key) override;

private:
	std::string text;
	bool buttonDown = false;
	bool hot = false;
};
