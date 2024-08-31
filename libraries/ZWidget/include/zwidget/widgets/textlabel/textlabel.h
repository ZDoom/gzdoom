
#pragma once

#include "../../core/widget.h"

enum TextLabelAlignment
{
	Left,
	Center,
	Right
};

class TextLabel : public Widget
{
public:
	TextLabel(Widget* parent = nullptr);

	void SetText(const std::string& value);
	const std::string& GetText() const;

	void SetTextAlignment(TextLabelAlignment alignment);
	TextLabelAlignment GetTextAlignment() const;

	double GetPreferredWidth() const;
	double GetPreferredHeight() const;

protected:
	void OnPaint(Canvas* canvas) override;

private:
	std::string text;
	TextLabelAlignment textAlignment = TextLabelAlignment::Left;
};
