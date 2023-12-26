
#pragma once

#include "../../core/widget.h"

class TextLabel : public Widget
{
public:
	TextLabel(Widget* parent = nullptr);

	void SetText(const std::string& value);
	const std::string& GetText() const;

	double GetPreferredHeight() const;

protected:
	void OnPaint(Canvas* canvas) override;

private:
	std::string text;
};
