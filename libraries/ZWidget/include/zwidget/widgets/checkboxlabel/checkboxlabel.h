
#pragma once

#include "../../core/widget.h"

class CheckboxLabel : public Widget
{
public:
	CheckboxLabel(Widget* parent = nullptr);

	void SetText(const std::string& value);
	const std::string& GetText() const;

	void SetChecked(bool value);
	bool GetChecked() const;
	void Toggle();

	double GetPreferredHeight() const;

protected:
	void OnPaint(Canvas* canvas) override;
	void OnMouseDown(const Point& pos, int key) override;
	void OnMouseUp(const Point& pos, int key) override;
	void OnMouseLeave() override;
	void OnKeyUp(EInputKey key) override;

private:
	std::string text;
	bool checked = false;
	bool mouseDownActive = false;
};
