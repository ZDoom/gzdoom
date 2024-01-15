
#include "widgets/lineedit/lineedit.h"
#include "core/utf8reader.h"
#include "core/colorf.h"

LineEdit::LineEdit(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(5.0, 3.0, 5.0, 3.0);

	timer = new Timer(this);
	timer->FuncExpired = [=]() { OnTimerExpired(); };

	scroll_timer = new Timer(this);
	scroll_timer->FuncExpired = [=]() { OnScrollTimerExpired(); };

	SetCursor(StandardCursor::ibeam);
}

LineEdit::~LineEdit()
{
}

bool LineEdit::IsReadOnly() const
{
	return readonly;
}

LineEdit::Alignment LineEdit::GetAlignment() const
{
	return align_left;
}

bool LineEdit::IsLowercase() const
{
	return lowercase;
}

bool LineEdit::IsUppercase() const
{
	return uppercase;
}

bool LineEdit::IsPasswordMode() const
{
	return password_mode;
}

int LineEdit::GetMaxLength() const
{
	return max_length;
}

std::string LineEdit::GetText() const
{
	return text;
}

int LineEdit::GetTextInt() const
{
	return std::atoi(text.c_str());
}

float LineEdit::GetTextFloat() const
{
	return (float)std::atof(text.c_str());
}

std::string LineEdit::GetSelection() const
{
	int start = std::min(selection_start, selection_start + selection_length);
	return text.substr(start, std::abs(selection_length));
}

int LineEdit::GetSelectionStart() const
{
	return selection_start;
}

int LineEdit::GetSelectionLength() const
{
	return selection_length;
}

int LineEdit::GetCursorPos() const
{
	return cursor_pos;
}

Size LineEdit::GetTextSize()
{
	Canvas* canvas = GetCanvas();
	return GetVisualTextSize(canvas);
}

Size LineEdit::GetTextSize(const std::string& str)
{
	Canvas* canvas = GetCanvas();
	return canvas->measureText(str).size();
}

double LineEdit::GetPreferredContentWidth()
{
	return GetTextSize().width;
}

double LineEdit::GetPreferredContentHeight(double width)
{
	return GetTextSize().height;
}

void LineEdit::SelectAll()
{
	SetTextSelection(0, (int)text.size());
	Update();
}

void LineEdit::SetReadOnly(bool enable)
{
	if (readonly != enable)
	{
		readonly = enable;
		Update();
	}
}

void LineEdit::SetAlignment(Alignment newalignment)
{
	if (alignment != newalignment)
	{
		alignment = newalignment;
		Update();
	}
}

void LineEdit::SetLowercase(bool enable)
{
	if (lowercase != enable)
	{
		lowercase = enable;
		text = ToLower(text);
		Update();
	}
}

void LineEdit::SetUppercase(bool enable)
{
	if (uppercase != enable)
	{
		uppercase = enable;
		text = ToUpper(text);
		Update();
	}
}

void LineEdit::SetPasswordMode(bool enable)
{
	if (password_mode != enable)
	{
		password_mode = enable;
		Update();
	}
}

void LineEdit::SetMaxLength(int length)
{
	if (max_length != length)
	{
		max_length = length;
		if ((int)text.length() > length)
		{
			if (FuncBeforeEditChanged)
				FuncBeforeEditChanged();
			text = text.substr(0, length);
			if (FuncAfterEditChanged)
				FuncAfterEditChanged();
		}
		Update();
	}
}

void LineEdit::SetText(const std::string& newtext)
{
	if (lowercase)
		text = ToLower(newtext);
	else if (uppercase)
		text = ToUpper(newtext);
	else
		text = newtext;

	clip_start_offset = 0;
	UpdateTextClipping();
	SetCursorPos((int)text.size());
	SetTextSelection(0, 0);
	Update();
}

void LineEdit::SetTextInt(int number)
{
	text = std::to_string(number);
	clip_start_offset = 0;
	UpdateTextClipping();
	SetCursorPos((int)text.size());
	SetTextSelection(0, 0);
	Update();
}

void LineEdit::SetTextFloat(float number, int num_decimal_places)
{
	text = ToFixed(number, num_decimal_places);
	clip_start_offset = 0;
	UpdateTextClipping();
	SetCursorPos((int)text.size());
	SetTextSelection(0, 0);
	Update();
}

void LineEdit::SetSelection(int pos, int length)
{
	//don't call FuncSelectionChanged() here, because this
	//member is for public usage
	selection_start = pos;
	selection_length = length;
	Update();
}

void LineEdit::ClearSelection()
{
	//don't call FuncSelectionChanged() here, because this
	//member is for public usage
	SetSelection(0, 0);
	Update();
}

void LineEdit::DeleteSelectedText()
{
	if (GetSelectionLength() == 0)
		return;

	int sel_start = selection_start;
	int sel_end = selection_start + selection_length;
	if (sel_start > sel_end)
		std::swap(sel_start, sel_end);

	text = text.substr(0, sel_start) + text.substr(sel_end, text.size());
	cursor_pos = sel_start;
	SetTextSelection(0, 0);
	int old_pos = GetCursorPos();
	SetCursorPos(0);
	SetCursorPos(old_pos);
}

void LineEdit::SetCursorPos(int pos)
{
	cursor_pos = pos;
	UpdateTextClipping();
	Update();
}

void LineEdit::SetInputMask(const std::string& mask)
{
	input_mask = mask;
}

void LineEdit::SetNumericMode(bool enable, bool decimals)
{
	numeric_mode = enable;
	numeric_mode_decimals = decimals;
}

void LineEdit::SetDecimalCharacter(const std::string& new_decimal_char)
{
	decimal_char = new_decimal_char;
}

void LineEdit::SetSelectAllOnFocusGain(bool enable)
{
	select_all_on_focus_gain = enable;
}

void LineEdit::OnMouseMove(const Point& pos)
{
	if (mouse_selecting && !ignore_mouse_events)
	{
		if (pos.x < 0.0 || pos.x >= GetWidth())
		{
			if (pos.x < 0.0)
				mouse_moves_left = true;
			else
				mouse_moves_left = false;

			if (!readonly)
				scroll_timer->Start(50, true);
		}
		else
		{
			scroll_timer->Stop();
			cursor_pos = GetCharacterIndex(pos.x);
			SetSelectionLength(cursor_pos - selection_start);
			Update();
		}
	}
}

bool LineEdit::OnMouseDown(const Point& pos, int key)
{
	if (key == IK_LeftMouse)
	{
		if (HasFocus())
		{
			CaptureMouse();
			mouse_selecting = true;
			cursor_pos = GetCharacterIndex(pos.x);
			SetTextSelection(cursor_pos, 0);
		}
		else
		{
			SetFocus();
		}
		Update();
	}
	return true;
}

bool LineEdit::OnMouseDoubleclick(const Point& pos, int key)
{
	return true;
}

bool LineEdit::OnMouseUp(const Point& pos, int key)
{
	if (mouse_selecting && key == IK_LeftMouse)
	{
		if (ignore_mouse_events) // This prevents text selection from changing from what was set when focus was gained.
		{
			ReleaseMouseCapture();
			ignore_mouse_events = false;
			mouse_selecting = false;
		}
		else
		{
			scroll_timer->Stop();
			ReleaseMouseCapture();
			mouse_selecting = false;
			int sel_end = GetCharacterIndex(pos.x);
			SetSelectionLength(sel_end - selection_start);
			cursor_pos = sel_end;
			SetFocus();
			Update();
		}
	}
	return true;
}

void LineEdit::OnKeyChar(std::string chars)
{
	if (FuncFilterKeyChar)
	{
		chars = FuncFilterKeyChar(chars);
		if (chars.empty())
			return;
	}

	if (!chars.empty() && !(chars[0] >= 0 && chars[0] < 32))
	{
		if (FuncBeforeEditChanged)
			FuncBeforeEditChanged();

		DeleteSelectedText();
		if (input_mask.empty())
		{
			if (numeric_mode)
			{
				// '-' can only be added once, and only as the first character.
				if (chars == "-" && cursor_pos == 0 && text.find("-") == std::string::npos)
				{
					if (InsertText(cursor_pos, chars))
						cursor_pos += (int)chars.size();
				}
				else if (numeric_mode_decimals && chars == decimal_char && cursor_pos > 0) // add decimal char 
				{
					if (text.find(decimal_char) == std::string::npos) // allow only one decimal char.
					{
						if (InsertText(cursor_pos, chars))
							cursor_pos += (int)chars.size();
					}
				}
				else if (numeric_mode_characters.find(chars) != std::string::npos) // 0-9
				{
					if (InsertText(cursor_pos, chars))
						cursor_pos += (int)chars.size();
				}
			}
			else
			{
				// not in any special mode, just insert the string.
				if (InsertText(cursor_pos, chars))
					cursor_pos += (int)chars.size();
			}
		}
		else
		{
			if (InputMaskAcceptsInput(cursor_pos, chars))
			{
				if (InsertText(cursor_pos, chars))
					cursor_pos += (int)chars.size();
			}
		}
		UpdateTextClipping();

		if (FuncAfterEditChanged)
			FuncAfterEditChanged();
	}
}

void LineEdit::OnKeyDown(EInputKey key)
{
	if (FuncIgnoreKeyDown && FuncIgnoreKeyDown(key))
		return;

	if (key == IK_Enter)
	{
		if (FuncEnterPressed)
			FuncEnterPressed();
		return;
	}

	if (!readonly)	// Do not flash cursor when readonly
	{
		cursor_blink_visible = true;
		timer->Start(500); // don't blink cursor when moving or typing.
	}

	if (key == IK_Enter || key == IK_Escape || key == IK_Tab)
	{
		// Do not consume these.
		return;
	}
	else if (key == IK_A && GetKeyState(IK_Ctrl))
	{
		// select all
		SetTextSelection(0, (int)text.size());
		cursor_pos = selection_length;
		UpdateTextClipping();
		Update();
	}
	else if (key == IK_C && GetKeyState(IK_Ctrl))
	{
		if (!password_mode)	// Do not allow copying the password to clipboard
		{
			std::string str = GetSelection();
			SetClipboardText(str);
		}
	}
	else if (readonly)
	{
		// Do not consume messages on read only component (only allow CTRL-A and CTRL-C)
		return;
	}
	else if (key == IK_Left)
	{
		Move(-1, GetKeyState(IK_Ctrl), GetKeyState(IK_Shift));
	}
	else if (key == IK_Right)
	{
		Move(1, GetKeyState(IK_Ctrl), GetKeyState(IK_Shift));
	}
	else if (key == IK_Backspace)
	{
		Backspace();
		UpdateTextClipping();
	}
	else if (key == IK_Delete)
	{
		Del();
		UpdateTextClipping();
	}
	else if (key == IK_Home)
	{
		SetSelectionStart(cursor_pos);
		cursor_pos = 0;
		if (GetKeyState(IK_Shift))
			SetSelectionLength(-selection_start);
		else
			SetTextSelection(0, 0);
		UpdateTextClipping();
		Update();
	}
	else if (key == IK_End)
	{
		SetSelectionStart(cursor_pos);
		cursor_pos = (int)text.size();
		if (GetKeyState(IK_Shift))
			SetSelectionLength((int)text.size() - selection_start);
		else
			SetTextSelection(0, 0);
		UpdateTextClipping();
		Update();
	}
	else if (key == IK_X && GetKeyState(IK_Ctrl))
	{
		std::string str = GetSelection();
		DeleteSelectedText();
		SetClipboardText(str);
		UpdateTextClipping();
	}
	else if (key == IK_V && GetKeyState(IK_Ctrl))
	{
		std::string str = GetClipboardText();
		std::string::const_iterator end_str = std::remove(str.begin(), str.end(), '\n');
		str.resize(end_str - str.begin());
		end_str = std::remove(str.begin(), str.end(), '\r');
		str.resize(end_str - str.begin());
		DeleteSelectedText();

		if (input_mask.empty())
		{
			if (numeric_mode)
			{
				std::string present_text = GetText();

				bool present_minus = present_text.find('-') != std::string::npos;
				bool str_minus = str.find('-') != std::string::npos;

				if (!present_minus || !str_minus)
				{
					if ((!present_minus && !str_minus) || //if no minus found
						(str_minus && cursor_pos == 0 && str[0] == '-') || //if there's minus in text to paste
						(present_minus && cursor_pos > 0)) //if there's minus in the beginning of control's text
					{
						if (numeric_mode_decimals)
						{
							std::string::size_type decimal_point_pos;
							if ((decimal_point_pos = str.find_first_not_of(numeric_mode_characters, str[0] == '-' ? 1 : 0)) == std::string::npos) //no decimal char inside string to paste
							{ //we don't look at the position of decimal char inside of text in the textbox, if it's present
								if (InsertText(cursor_pos, str))
									SetCursorPos(cursor_pos + (int)str.length());
							}
							else
							{
								if (present_text.find(decimal_char) == std::string::npos &&
									str[decimal_point_pos] == decimal_char[0] &&
									str.find_first_not_of(numeric_mode_characters, decimal_point_pos + 1) == std::string::npos) //allow only one decimal char in the string to paste
								{
									if (InsertText(cursor_pos, str))
										SetCursorPos(cursor_pos + (int)str.length());
								}
							}
						}
						else
						{
							if (str.find_first_not_of(numeric_mode_characters, str[0] == '-' ? 1 : 0) == std::string::npos)
							{
								if (InsertText(cursor_pos, str))
									SetCursorPos(cursor_pos + (int)str.length());
							}
						}
					}
				}
			}
			else
			{
				if (InsertText(cursor_pos, str))
					SetCursorPos(cursor_pos + (int)str.length());
			}
		}
		else
		{
			if (InputMaskAcceptsInput(cursor_pos, str))
			{
				if (InsertText(cursor_pos, str))
					SetCursorPos(cursor_pos + (int)str.length());
			}
		}

		UpdateTextClipping();
	}
	else if (GetKeyState(IK_Ctrl) && key == IK_Z)
	{
		if (!readonly)
		{
			std::string tmp = undo_info.undo_text;
			undo_info.undo_text = GetText();
			SetText(tmp);
		}
	}
	else if (key == IK_Shift)
	{
		if (selection_start == -1)
			SetTextSelection(cursor_pos, 0);
	}

	if (FuncAfterEditChanged)
		FuncAfterEditChanged();
}

void LineEdit::OnKeyUp(EInputKey key)
{
}

void LineEdit::OnSetFocus()
{
	if (!readonly)
		timer->Start(500);
	if (select_all_on_focus_gain)
		SelectAll();
	ignore_mouse_events = true;
	cursor_pos = (int)text.length();

	Update();

	if (FuncFocusGained)
		FuncFocusGained();
}

void LineEdit::OnLostFocus()
{
	timer->Stop();
	SetTextSelection(0, 0);

	Update();

	if (FuncFocusLost)
		FuncFocusLost();
}

void LineEdit::Move(int steps, bool ctrl, bool shift)
{
	if (shift && selection_length == 0)
		SetSelectionStart(cursor_pos);

	// Jump over words if control is pressed.
	if (ctrl)
	{
		if (steps < 0)
			steps = FindPreviousBreakCharacter(cursor_pos - 1) - cursor_pos;
		else
			steps = FindNextBreakCharacter(cursor_pos + 1) - cursor_pos;

		cursor_pos += steps;
		if (cursor_pos < 0)
			cursor_pos = 0;
		if (cursor_pos > (int)text.size())
			cursor_pos = (int)text.size();
	}
	else
	{
		UTF8Reader utf8_reader(text.data(), text.length());
		utf8_reader.set_position(cursor_pos);
		if (steps > 0)
		{
			for (int i = 0; i < steps; i++)
				utf8_reader.next();
		}
		else if (steps < 0)
		{
			for (int i = 0; i < -steps; i++)
				utf8_reader.prev();
		}

		cursor_pos = (int)utf8_reader.position();
	}


	// Clear the selection if a cursor key is pressed but shift isn't down. 
	if (shift)
		SetSelectionLength(cursor_pos - selection_start);
	else
		SetTextSelection(-1, 0);

	UpdateTextClipping();

	Update();

	undo_info.first_text_insert = true;
}

bool LineEdit::InsertText(int pos, const std::string& str)
{
	undo_info.first_erase = false;
	if (undo_info.first_text_insert)
	{
		undo_info.undo_text = GetText();
		undo_info.first_text_insert = false;
	}

	// checking if insert exceeds max length
	if (UTF8Reader::utf8_length(text) + UTF8Reader::utf8_length(str) > max_length)
	{
		return false;
	}

	if (lowercase)
		text.insert(pos, ToLower(str));
	else if (uppercase)
		text.insert(pos, ToUpper(str));
	else
		text.insert(pos, str);

	UpdateTextClipping();
	Update();
	return true;
}

void LineEdit::Backspace()
{
	if (undo_info.first_erase)
	{
		undo_info.first_erase = false;
		undo_info.undo_text = GetText();
	}

	if (GetSelectionLength() != 0)
	{
		DeleteSelectedText();
		Update();
	}
	else
	{
		if (cursor_pos > 0)
		{
			UTF8Reader utf8_reader(text.data(), text.length());
			utf8_reader.set_position(cursor_pos);
			utf8_reader.prev();
			size_t length = utf8_reader.char_length();
			text.erase(cursor_pos - length, length);
			cursor_pos -= (int)length;
			Update();
		}
	}

	int old_pos = GetCursorPos();
	SetCursorPos(0);
	SetCursorPos(old_pos);
}

void LineEdit::Del()
{
	if (undo_info.first_erase)
	{
		undo_info.first_erase = false;
		undo_info.undo_text = GetText();
	}

	if (GetSelectionLength() != 0)
	{
		DeleteSelectedText();
		Update();
	}
	else
	{
		if (cursor_pos < (int)text.size())
		{
			UTF8Reader utf8_reader(text.data(), text.length());
			utf8_reader.set_position(cursor_pos);
			size_t length = utf8_reader.char_length();
			text.erase(cursor_pos, length);
			Update();
		}
	}
}

int LineEdit::GetCharacterIndex(double mouse_x)
{
	if (text.size() <= 1)
	{
		return (int)text.size();
	}

	Canvas* canvas = GetCanvas();
	UTF8Reader utf8_reader(text.data(), text.length());

	int seek_start = clip_start_offset;
	int seek_end = (int)text.size();
	int seek_center = (seek_start + seek_end) / 2;

	//fast search
	while (true)
	{
		utf8_reader.set_position(seek_center);
		utf8_reader.move_to_leadbyte();

		seek_center = (int)utf8_reader.position();

		Size text_size = GetVisualTextSize(canvas, clip_start_offset, seek_center - clip_start_offset);

		if (text_size.width > mouse_x)
			seek_end = seek_center;
		else
			seek_start = seek_center;

		if (seek_end - seek_start < 7)
			break; //go to accurate search

		seek_center = (seek_start + seek_end) / 2;
	}

	utf8_reader.set_position(seek_start);
	utf8_reader.move_to_leadbyte();

	//accurate search
	while (true)
	{
		seek_center = (int)utf8_reader.position();

		Size text_size = GetVisualTextSize(canvas, clip_start_offset, seek_center - clip_start_offset);
		if (text_size.width > mouse_x || utf8_reader.is_end())
			break;

		utf8_reader.next();
	}

	return seek_center;
}

void LineEdit::UpdateTextClipping()
{
	Canvas* canvas = GetCanvas();

	Size text_size = GetVisualTextSize(canvas, clip_start_offset, (int)text.size() - clip_start_offset);

	if (cursor_pos < clip_start_offset)
		clip_start_offset = cursor_pos;

	Rect cursor_rect = GetCursorRect();

	UTF8Reader utf8_reader(text.data(), text.length());
	double width = GetWidth();
	while (cursor_rect.x + cursor_rect.width > width)
	{
		utf8_reader.set_position(clip_start_offset);
		utf8_reader.next();
		clip_start_offset = (int)utf8_reader.position();
		if (clip_start_offset == text.size())
			break;
		cursor_rect = GetCursorRect();
	}

	// Get number of chars of current text fitting in the lineedit.
	int search_upper = (int)text.size();
	int search_lower = clip_start_offset;

	while (true)
	{
		int midpoint = (search_lower + search_upper) / 2;

		utf8_reader.set_position(midpoint);
		utf8_reader.move_to_leadbyte();
		if (midpoint != utf8_reader.position())
			utf8_reader.next();
		midpoint = (int)utf8_reader.position();

		if (midpoint == search_lower || midpoint == search_upper)
			break;

		Size midpoint_size = GetVisualTextSize(canvas, clip_start_offset, midpoint - clip_start_offset);

		if (width < midpoint_size.width)
			search_upper = midpoint;
		else
			search_lower = midpoint;
	}
	clip_end_offset = search_upper;

	if (cursor_rect.x < 0.0)
	{
		clip_start_offset = cursor_pos;
	}
}

Rect LineEdit::GetCursorRect()
{
	Canvas* canvas = GetCanvas();

	int substr_end = cursor_pos - clip_start_offset;
	if (substr_end < 0)
		substr_end = 0;

	std::string clipped_text = text.substr(clip_start_offset, substr_end);

	if (password_mode)
	{
		// If we are in password mode, we gonna return the right characters
		clipped_text = CreatePassword(UTF8Reader::utf8_length(clipped_text));
	}

	Size text_size_before_cursor = canvas->measureText(clipped_text).size();

	Rect cursor_rect;
	cursor_rect.x = text_size_before_cursor.width;
	cursor_rect.width = 1.0f;

	cursor_rect.y = vertical_text_align.top;
	cursor_rect.height = vertical_text_align.bottom - vertical_text_align.top;

	return cursor_rect;
}

Rect LineEdit::GetSelectionRect()
{
	Canvas* canvas = GetCanvas();

	// text before selection:

	std::string txt_before = GetVisibleTextBeforeSelection();
	Size text_size_before_selection = canvas->measureText(txt_before).size();

	// selection text:
	std::string txt_selected = GetVisibleSelectedText();
	Size text_size_selection = canvas->measureText(txt_selected).size();

	Rect selection_rect;
	selection_rect.x = text_size_before_selection.width;
	selection_rect.width = text_size_selection.width;
	selection_rect.y = vertical_text_align.top;
	selection_rect.height = vertical_text_align.bottom - vertical_text_align.top;
	return selection_rect;
}

int LineEdit::FindNextBreakCharacter(int search_start)
{
	if (search_start >= int(text.size()) - 1)
		return (int)text.size();

	size_t pos = text.find_first_of(break_characters, search_start);
	if (pos == std::string::npos)
		return (int)text.size();
	return (int)pos;
}

int LineEdit::FindPreviousBreakCharacter(int search_start)
{
	if (search_start <= 0)
		return 0;
	size_t pos = text.find_last_of(break_characters, search_start);
	if (pos == std::string::npos)
		return 0;
	return (int)pos;
}

void LineEdit::OnTimerExpired()
{
	if (!IsVisible())
	{
		timer->Stop();
		return;
	}

	if (cursor_blink_visible)
		timer->Start(500);
	else
		timer->Start(500);

	cursor_blink_visible = !cursor_blink_visible;
	Update();
}

void LineEdit::OnGeometryChanged()
{
	Canvas* canvas = GetCanvas();

	vertical_text_align = canvas->verticalTextAlign();

	clip_start_offset = 0;
	cursor_pos = 0;
	UpdateTextClipping();
}

std::string LineEdit::GetVisibleTextBeforeSelection()
{
	std::string ret;
	int sel_start = std::min(selection_start, selection_start + selection_length);
	int start = std::min(sel_start, clip_start_offset);

	if (start < clip_start_offset)
		return ret;

	int end = std::min(sel_start, clip_end_offset);

	ret = text.substr(start, end - start);

	// If we are in password mode, we gonna return the right characters
	if (password_mode)
		ret = CreatePassword(UTF8Reader::utf8_length(ret));

	return ret;
}

std::string LineEdit::GetVisibleSelectedText()
{
	std::string ret;

	if (selection_length == 0)
		return ret;

	int sel_start = std::min(selection_start, selection_start + selection_length);
	int sel_end = std::max(selection_start, selection_start + selection_length);
	int end = std::min(clip_end_offset, sel_end);
	int start = std::max(clip_start_offset, sel_start);

	if (start > end)
		return ret;

	if (start == end)
		return ret;

	ret = text.substr(start, end - start);

	// If we are in password mode, we gonna return the right characters
	if (password_mode)
		ret = CreatePassword(UTF8Reader::utf8_length(ret));

	return ret;
}

void LineEdit::SetSelectionStart(int start)
{
	if (FuncSelectionChanged && selection_length && selection_start != start)
		FuncSelectionChanged();

	selection_start = start;
}

void LineEdit::SetSelectionLength(int length)
{
	if (FuncSelectionChanged && selection_length != length)
		FuncSelectionChanged();

	selection_length = length;
}

void LineEdit::SetTextSelection(int start, int length)
{
	if (FuncSelectionChanged && (selection_length != length || (selection_length && selection_start != start)))
		FuncSelectionChanged();

	selection_start = start;
	selection_length = length;
}

std::string LineEdit::GetVisibleTextAfterSelection()
{
	// returns the whole visible string if there is no selection.
	std::string ret;

	int sel_end = std::max(selection_start, selection_start + selection_length);
	int start = std::max(clip_start_offset, sel_end);

	int end = clip_end_offset;
	if (start > end)
		return ret;

	if (clip_end_offset == sel_end)
		return ret;

	if (sel_end <= 0)
		return ret;
	else
	{
		ret = text.substr(start, end - start);
		// If we are in password mode, we gonna return the right characters
		if (password_mode)
			ret = CreatePassword(UTF8Reader::utf8_length(ret));

		return ret;
	}
}

void LineEdit::OnPaintFrame(Canvas* canvas)
{
	double w = GetFrameGeometry().width;
	double h = GetFrameGeometry().height;
	Colorf bordercolor = Colorf::fromRgba8(100, 100, 100);
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, h), Colorf::fromRgba8(38, 38, 38));
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, 1.0), bordercolor);
	canvas->fillRect(Rect::xywh(0.0, h - 1.0, w, 1.0), bordercolor);
	canvas->fillRect(Rect::xywh(0.0, 0.0, 1.0, h - 0.0), bordercolor);
	canvas->fillRect(Rect::xywh(w - 1.0, 0.0, 1.0, h - 0.0), bordercolor);
}

void LineEdit::OnPaint(Canvas* canvas)
{
	std::string txt_before = GetVisibleTextBeforeSelection();
	std::string txt_selected = GetVisibleSelectedText();
	std::string txt_after = GetVisibleTextAfterSelection();

	if (txt_before.empty() && txt_selected.empty() && txt_after.empty())
	{
		txt_after = text.substr(clip_start_offset, clip_end_offset - clip_start_offset);

		// If we are in password mode, we gonna return the right characters
		if (password_mode)
			txt_after = CreatePassword(UTF8Reader::utf8_length(txt_after));
	}

	Size size_before = canvas->measureText(txt_before).size();
	Size size_selected = canvas->measureText(txt_selected).size();

	if (!txt_selected.empty())
	{
		// Draw selection box.
		Rect selection_rect = GetSelectionRect();
		canvas->fillRect(selection_rect, HasFocus() ? Colorf::fromRgba8(100, 100, 100) : Colorf::fromRgba8(68, 68, 68));
	}

	// Draw text before selection
	if (!txt_before.empty())
	{
		canvas->drawText(Point(0.0, canvas->verticalTextAlign().baseline), Colorf::fromRgba8(255, 255, 255), txt_before);
	}
	if (!txt_selected.empty())
	{
		canvas->drawText(Point(size_before.width, canvas->verticalTextAlign().baseline), Colorf::fromRgba8(255, 255, 255), txt_selected);
	}
	if (!txt_after.empty())
	{
		canvas->drawText(Point(size_before.width + size_selected.width, canvas->verticalTextAlign().baseline), Colorf::fromRgba8(255, 255, 255), txt_after);
	}

	// draw cursor
	if (HasFocus())
	{
		if (cursor_blink_visible)
		{
			Rect cursor_rect = GetCursorRect();
			canvas->fillRect(cursor_rect, Colorf::fromRgba8(255, 255, 255));
		}
	}
}

void LineEdit::OnScrollTimerExpired()
{
	if (mouse_moves_left)
		Move(-1, false, false);
	else
		Move(1, false, false);
}

void LineEdit::OnEnableChanged()
{
	bool enabled = IsEnabled();

	if (!enabled)
	{
		cursor_blink_visible = false;
		timer->Stop();
	}
	Update();
}

bool LineEdit::InputMaskAcceptsInput(int cursor_pos, const std::string& str)
{
	return str.find_first_not_of(input_mask) == std::string::npos;
}

std::string LineEdit::CreatePassword(std::string::size_type num_letters) const
{
	return std::string(num_letters, '*');
}

Size LineEdit::GetVisualTextSize(Canvas* canvas, int pos, int npos) const
{
	return canvas->measureText(password_mode ? CreatePassword(UTF8Reader::utf8_length(text.substr(pos, npos))) : text.substr(pos, npos)).size();
}

Size LineEdit::GetVisualTextSize(Canvas* canvas) const
{
	return canvas->measureText(password_mode ? CreatePassword(UTF8Reader::utf8_length(text)) : text).size();
}

std::string LineEdit::ToFixed(float number, int num_decimal_places)
{
	for (int i = 0; i < num_decimal_places; i++)
		number *= 10.0f;
	std::string val = std::to_string((int)std::round(number));
	if ((int)val.size() < num_decimal_places)
		val.resize(num_decimal_places + 1, 0);
	return val.substr(0, val.size() - num_decimal_places) + "." + val.substr(val.size() - num_decimal_places);
}

std::string LineEdit::ToLower(const std::string& text)
{
	return text;
}

std::string LineEdit::ToUpper(const std::string& text)
{
	return text;
}

const std::string LineEdit::break_characters = " ::;,.-";
const std::string LineEdit::numeric_mode_characters = "0123456789";
