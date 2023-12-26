
#pragma once

#include "../../core/widget.h"

class PushButton : public Widget
{
public:
	PushButton(Widget* parent = nullptr);

	void SetText(const std::string& value);
	const std::string& GetText() const;

	double GetPreferredHeight() const;

protected:
	void OnPaintFrame(Canvas* canvas) override;
	void OnPaint(Canvas* canvas) override;

private:
	std::string text;
};
