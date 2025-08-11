
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
	void OnPaint(Canvas* canvas) override;
	bool OnMouseDown(const Point& pos, InputKey key) override;
	bool OnMouseUp(const Point& pos, InputKey key) override;
	void OnMouseMove(const Point& pos) override;
	void OnMouseLeave() override;
	void OnKeyDown(InputKey key) override;
	void OnKeyUp(InputKey key) override;

private:
	std::string text;
};
