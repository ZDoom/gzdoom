#pragma once

#include <zwidget/core/widget.h>

class TextEdit;

class ErrorWindow : public Widget
{
public:
	static void ExecModal(const std::string& text);

protected:
	void OnGeometryChanged() override;

private:
	ErrorWindow();

	void SetText(const std::string& text);

	TextEdit* LogView = nullptr;
};
