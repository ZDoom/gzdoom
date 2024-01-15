
#pragma once

#include "../../core/widget.h"
#include "../../core/timer.h"
#include "../../core/span_layout.h"
#include "../../core/font.h"
#include <functional>

class Scrollbar;

class TextEdit : public Widget
{
public:
	TextEdit(Widget* parent);
	~TextEdit();

	bool IsReadOnly() const;
	bool IsLowercase() const;
	bool IsUppercase() const;
	int GetMaxLength() const;
	std::string GetText() const;
	int GetLineCount() const;
	std::string GetLineText(int line) const;
	std::string GetSelection() const;
	int GetSelectionStart() const;
	int GetSelectionLength() const;
	int GetCursorPos() const;
	int GetCursorLineNumber() const;
	double GetTotalHeight();

	void SetSelectAllOnFocusGain(bool enable);
	void SelectAll();
	void SetReadOnly(bool enable = true);
	void SetLowercase(bool enable = true);
	void SetUppercase(bool enable = true);
	void SetMaxLength(int length);
	void SetText(const std::string& text);
	void AddText(const std::string& text);
	void SetSelection(int pos, int length);
	void ClearSelection();
	void SetCursorPos(int pos);
	void DeleteSelectedText();
	void SetInputMask(const std::string& mask);
	void SetCursorDrawingEnabled(bool enable);

	std::function<std::string(std::string text)> FuncFilterKeyChar;
	std::function<void()> FuncBeforeEditChanged;
	std::function<void()> FuncAfterEditChanged;
	std::function<void()> FuncSelectionChanged;
	std::function<void()> FuncFocusGained;
	std::function<void()> FuncFocusLost;
	std::function<void()> FuncEnterPressed;

protected:
	void OnPaintFrame(Canvas* canvas) override;
	void OnPaint(Canvas* canvas) override;
	void OnMouseMove(const Point& pos) override;
	bool OnMouseDown(const Point& pos, int key) override;
	bool OnMouseDoubleclick(const Point& pos, int key) override;
	bool OnMouseUp(const Point& pos, int key) override;
	void OnKeyChar(std::string chars) override;
	void OnKeyDown(EInputKey key) override;
	void OnKeyUp(EInputKey key) override;
	void OnGeometryChanged() override;
	void OnEnableChanged() override;
	void OnSetFocus() override;
	void OnLostFocus() override;

private:
	void LayoutLines(Canvas* canvas);

	void OnTimerExpired();
	void OnScrollTimerExpired();
	void CreateComponents();
	void OnVerticalScroll();
	void UpdateVerticalScroll();
	void MoveVerticalScroll();
	double GetTotalLineHeight();

	struct Line
	{
		std::string text;
		SpanLayout layout;
		Rect box;
		bool invalidated = true;
	};

	struct ivec2
	{
		ivec2() = default;
		ivec2(int x, int y) : x(x), y(y) { }
		int x = 0;
		int y = 0;

		bool operator==(const ivec2& b) const { return x == b.x && y == b.y; }
		bool operator!=(const ivec2& b) const { return x != b.x || y != b.y; }
	};

	Scrollbar* vert_scrollbar;
	Timer* timer = nullptr;
	std::vector<Line> lines = { Line() };
	ivec2 cursor_pos = { 0, 0 };
	int max_length = -1;
	bool mouse_selecting = false;
	bool lowercase = false;
	bool uppercase = false;
	bool readonly = false;
	ivec2 selection_start = { -1, 0 };
	int selection_length = 0;
	std::string input_mask;

	static std::string break_characters;

	void Move(int steps, bool shift, bool ctrl);
	void InsertText(ivec2 pos, const std::string& str);
	void Backspace();
	void Del();
	ivec2 GetCharacterIndex(Point mouse_wincoords);
	ivec2 FindNextBreakCharacter(ivec2 pos);
	ivec2 FindPreviousBreakCharacter(ivec2 pos);
	bool InputMaskAcceptsInput(ivec2 cursor_pos, const std::string& str);

	std::string::size_type ToOffset(ivec2 pos) const;
	ivec2 FromOffset(std::string::size_type offset) const;

	VerticalTextPosition vertical_text_align;
	Timer* scroll_timer = nullptr;

	bool mouse_moves_left = false;
	bool cursor_blink_visible = true;
	unsigned int blink_timer = 0;
	int clip_start_offset = 0;
	int clip_end_offset = 0;
	bool ignore_mouse_events = false;

	struct UndoInfo
	{
		/* set undo text when:
		   - added char after moving
		   - destructive block operation (del, cut etc)
		   - beginning erase
		*/

		std::string undo_text;
		bool first_erase = false;
		bool first_text_insert = false;
	} undo_info;

	bool select_all_on_focus_gain = false;

	std::shared_ptr<Font> font = Font::Create("NotoSans", 12.0);

	template<typename T>
	static T clamp(T val, T minval, T maxval) { return std::max<T>(std::min<T>(val, maxval), minval); }
};
