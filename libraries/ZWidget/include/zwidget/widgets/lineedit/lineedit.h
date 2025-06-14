
#pragma once

#include "../../core/widget.h"
#include "../../core/timer.h"
#include <functional>

class LineEdit : public Widget
{
public:
	LineEdit(Widget* parent);
	~LineEdit();

	enum Alignment
	{
		align_left,
		align_center,
		align_right
	};

	Alignment GetAlignment() const;
	bool IsReadOnly() const;
	bool IsLowercase() const;
	bool IsUppercase() const;
	bool IsPasswordMode() const;
	int GetMaxLength() const;

	std::string GetText() const;
	int GetTextInt() const;
	float GetTextFloat() const;

	std::string GetSelection() const;
	int GetSelectionStart() const;
	int GetSelectionLength() const;

	int GetCursorPos() const;
	Size GetTextSize();

	Size GetTextSize(const std::string& str);
	double GetPreferredContentWidth();
	double GetPreferredContentHeight(double width);

	void SetSelectAllOnFocusGain(bool enable);
	void SelectAll();
	void SetAlignment(Alignment alignment);
	void SetReadOnly(bool enable = true);
	void SetLowercase(bool enable = true);
	void SetUppercase(bool enable = true);
	void SetPasswordMode(bool enable = true);
	void SetNumericMode(bool enable = true, bool decimals = false);
	void SetMaxLength(int length);
	void SetText(const std::string& text);
	void SetTextInt(int number);
	void SetTextFloat(float number, int num_decimal_places = 6);
	void SetSelection(int pos, int length);
	void ClearSelection();
	void SetCursorPos(int pos);
	void DeleteSelectedText();
	void SetInputMask(const std::string& mask);
	void SetDecimalCharacter(const std::string& decimal_char);

	std::function<bool(InputKey key)> FuncIgnoreKeyDown;
	std::function<std::string(std::string text)> FuncFilterKeyChar;
	std::function<void()> FuncBeforeEditChanged;
	std::function<void()> FuncAfterEditChanged;
	std::function<void()> FuncSelectionChanged;
	std::function<void()> FuncFocusGained;
	std::function<void()> FuncFocusLost;
	std::function<void()> FuncEnterPressed;

protected:
	void OnPaint(Canvas* canvas) override;
	void OnMouseMove(const Point& pos) override;
	bool OnMouseDown(const Point& pos, InputKey key) override;
	bool OnMouseDoubleclick(const Point& pos, InputKey key) override;
	bool OnMouseUp(const Point& pos, InputKey key) override;
	void OnKeyChar(std::string chars) override;
	void OnKeyDown(InputKey key) override;
	void OnKeyUp(InputKey key) override;
	void OnGeometryChanged() override;
	void OnEnableChanged() override;
	void OnSetFocus() override;
	void OnLostFocus() override;

private:
	void OnTimerExpired();
	void OnScrollTimerExpired();
	void UpdateTextClipping();

	void Move(int steps, bool ctrl, bool shift);
	bool InsertText(int pos, const std::string& str);
	void Backspace();
	void Del();
	int GetCharacterIndex(double x);
	int FindNextBreakCharacter(int pos);
	int FindPreviousBreakCharacter(int pos);
	std::string GetVisibleTextBeforeSelection();
	std::string GetVisibleTextAfterSelection();
	std::string GetVisibleSelectedText();
	std::string CreatePassword(std::string::size_type num_letters) const;
	Size GetVisualTextSize(Canvas* canvas, int pos, int npos) const;
	Size GetVisualTextSize(Canvas* canvas) const;
	Rect GetCursorRect();
	Rect GetSelectionRect();
	bool InputMaskAcceptsInput(int cursor_pos, const std::string& str);
	void SetSelectionStart(int start);
	void SetSelectionLength(int length);
	void SetTextSelection(int start, int length);

	static std::string ToFixed(float number, int num_decimal_places);
	static std::string ToLower(const std::string& text);
	static std::string ToUpper(const std::string& text);

	Timer* timer = nullptr;
	std::string text;
	Alignment alignment = align_left;
	int cursor_pos = 0;
	int max_length = -1;
	bool mouse_selecting = false;
	bool lowercase = false;
	bool uppercase = false;
	bool password_mode = false;
	bool numeric_mode = false;
	bool numeric_mode_decimals = false;
	bool readonly = false;
	int selection_start = -1;
	int selection_length = 0;
	std::string input_mask;
	std::string decimal_char = ".";

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
	};
	
	UndoInfo undo_info;

	bool select_all_on_focus_gain = true;

	static const std::string break_characters;
	static const std::string numeric_mode_characters;
};
