#pragma once

#include <zwidget/core/widget.h>
#include <zwidget/core/span_layout.h>

class LogViewer;
class PushButton;
class Scrollbar;

class ErrorWindow : public Widget
{
public:
	static bool ExecModal(const std::string& text, const std::string& log, std::vector<uint8_t> minidump = {});

	ErrorWindow(std::vector<uint8_t> minidump);

	bool Restart = false;

protected:
	void OnClose() override;
	void OnGeometryChanged() override;

private:
	void SetText(const std::string& text, const std::string& log);

	void OnClipboardButtonClicked();
	void OnRestartButtonClicked();
	void OnSaveReportButtonClicked();

	LogViewer* LogView = nullptr;
	PushButton* ClipboardButton = nullptr;
	PushButton* RestartButton = nullptr;
	PushButton* SaveReportButton = nullptr;

	std::vector<uint8_t> minidump;
	std::string clipboardtext;
};

class LogViewer : public Widget
{
public:
	LogViewer(Widget* parent);

	void SetText(const std::string& text, const std::string& log);

protected:
	void OnPaintFrame(Canvas* canvas) override;
	void OnPaint(Canvas* canvas) override;
	bool OnMouseWheel(const Point& pos, InputKey key) override;
	void OnKeyDown(InputKey key) override;
	void OnGeometryChanged() override;

private:
	void OnScrollbarScroll();
	void ScrollUp(int lines);
	void ScrollDown(int lines);

	SpanLayout CreateLineLayout(const std::string& text);

	Scrollbar* scrollbar = nullptr;

	std::shared_ptr<Font> largefont = Font::Create("Poppins", 16.0);
	std::shared_ptr<Font> font = Font::Create("Poppins", 12.0);
	std::vector<SpanLayout> lines;
};
