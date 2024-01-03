
#include "widgets/textedit/textedit.h"
#include "widgets/scrollbar/scrollbar.h"
#include "core/utf8reader.h"
#include "core/colorf.h"

#ifdef _MSC_VER
#pragma warning(disable: 4267) // warning C4267: 'initializing': conversion from 'size_t' to 'int', possible loss of data
#endif

TextEdit::TextEdit(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(8.0, 8.0, 8.0, 8.0);

	timer = new Timer(this);
	timer->FuncExpired = [=]() { OnTimerExpired(); };

	scroll_timer = new Timer(this);
	scroll_timer->FuncExpired = [=]() { OnScrollTimerExpired(); };

	SetCursor(StandardCursor::ibeam);

	CreateComponents();
}

TextEdit::~TextEdit()
{
}

bool TextEdit::IsReadOnly() const
{
	return readonly;
}

bool TextEdit::IsLowercase() const
{
	return lowercase;
}

bool TextEdit::IsUppercase() const
{
	return uppercase;
}

int TextEdit::GetMaxLength() const
{
	return max_length;
}

std::string TextEdit::GetLineText(int line) const
{
	if (line >= 0 && line < (int)lines.size())
		return lines[line].text;
	else
		return std::string();
}

std::string TextEdit::GetText() const
{
	std::string::size_type size = 0;
	for (size_t i = 0; i < lines.size(); i++)
		size += lines[i].text.size();
	size += lines.size() - 1;

	std::string text;
	text.reserve(size);

	for (size_t i = 0; i < lines.size(); i++)
	{
		if (i > 0)
			text.push_back('\n');
		text += lines[i].text;
	}

	return text;
}

int TextEdit::GetLineCount() const
{
	return (int)lines.size();
}

std::string TextEdit::GetSelection() const
{
	std::string::size_type offset = ToOffset(selection_start);
	int start = (int)std::min(offset, offset + selection_length);
	return GetText().substr(start, abs(selection_length));
}

int TextEdit::GetSelectionStart() const
{
	return (int)ToOffset(selection_start);
}

int TextEdit::GetSelectionLength() const
{
	return selection_length;
}

int TextEdit::GetCursorPos() const
{
	return (int)ToOffset(cursor_pos);
}

int TextEdit::GetCursorLineNumber() const
{
	return cursor_pos.y;
}

void TextEdit::SelectAll()
{
	std::string::size_type size = 0;
	for (size_t i = 0; i < lines.size(); i++)
		size += lines[i].text.size();
	size += lines.size() - 1;
	SetSelection(0, (int)size);
}

void TextEdit::SetReadOnly(bool enable)
{
	if (readonly != enable)
	{
		readonly = enable;
		Update();
	}
}

void TextEdit::SetLowercase(bool enable)
{
	if (lowercase != enable)
	{
		lowercase = enable;
		Update();
	}
}

void TextEdit::SetUppercase(bool enable)
{
	if (uppercase != enable)
	{
		uppercase = enable;
		Update();
	}
}

void TextEdit::SetMaxLength(int length)
{
	if (max_length != length)
	{
		max_length = length;

		std::string::size_type size = 0;
		for (size_t i = 0; i < lines.size(); i++)
			size += lines[i].text.size();
		size += lines.size() - 1;

		if ((int)size > length)
		{
			if (FuncBeforeEditChanged)
				FuncBeforeEditChanged();
			SetSelection(length, (int)size - length);
			DeleteSelectedText();
			if (FuncAfterEditChanged)
				FuncAfterEditChanged();
		}
		Update();
	}
}

void TextEdit::SetText(const std::string& text)
{
	lines.clear();
	std::string::size_type start = 0;
	std::string::size_type end = text.find('\n');
	while (end != std::string::npos)
	{
		TextEdit::Line line;
		line.text = text.substr(start, end - start);
		lines.push_back(line);
		start = end + 1;
		end = text.find('\n', start);
	}
	TextEdit::Line line;
	line.text = text.substr(start);
	lines.push_back(line);

	clip_start_offset = 0;
	SetCursorPos(0);
	ClearSelection();
	Update();
}

void TextEdit::AddText(const std::string& text)
{
	std::string::size_type start = 0;
	std::string::size_type end = text.find('\n');
	while (end != std::string::npos)
	{
		TextEdit::Line line;
		line.text = text.substr(start, end - start);
		lines.push_back(line);
		start = end + 1;
		end = text.find('\n', start);
	}
	TextEdit::Line line;
	line.text = text.substr(start);
	lines.push_back(line);

	//	clip_start_offset = 0;
	//	SetCursorPos(0);
	ClearSelection();
	Update();
}

void TextEdit::SetSelection(int pos, int length)
{
	selection_start = ivec2(pos, 0);
	selection_length = length;
	Update();
}

void TextEdit::ClearSelection()
{
	SetSelection(0, 0);
	Update();
}

void TextEdit::DeleteSelectedText()
{
	if (GetSelectionLength() == 0)
		return;

	std::string::size_type offset = ToOffset(selection_start);
	int start = (int)std::min(offset, offset + selection_length);
	int length = std::abs(selection_length);

	ClearSelection();
	std::string text = GetText();
	SetText(text.substr(0, start) + text.substr(start + length));
	SetCursorPos(start);
}

void TextEdit::SetCursorPos(int pos)
{
	cursor_pos = FromOffset(pos);
	Update();
}

void TextEdit::SetInputMask(const std::string& mask)
{
	input_mask = mask;
}

void TextEdit::SetCursorDrawingEnabled(bool enable)
{
	if (!readonly)
		timer->Start(500);
}

void TextEdit::SetSelectAllOnFocusGain(bool enable)
{
	select_all_on_focus_gain = enable;
}

void TextEdit::OnMouseMove(const Point& pos)
{
	if (mouse_selecting && !ignore_mouse_events)
	{
		if (pos.x < 0.0 || pos.x > GetWidth())
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
			cursor_pos = GetCharacterIndex(pos);
			selection_length = ToOffset(cursor_pos) - ToOffset(selection_start);
			Update();
		}
	}
}

void TextEdit::OnMouseDown(const Point& pos, int key)
{
	if (key == IK_LeftMouse)
	{
		CaptureMouse();
		mouse_selecting = true;
		cursor_pos = GetCharacterIndex(pos);
		selection_start = cursor_pos;
		selection_length = 0;

		Update();
	}
}

void TextEdit::OnMouseDoubleclick(const Point& pos, int key)
{
}

void TextEdit::OnMouseUp(const Point& pos, int key)
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
			ivec2 sel_end = GetCharacterIndex(pos);
			selection_length = ToOffset(sel_end) - ToOffset(selection_start);
			cursor_pos = sel_end;
			SetFocus();
			Update();
		}
	}
}

void TextEdit::OnKeyChar(std::string chars)
{
	if (!chars.empty() && !(chars[0] >= 0 && chars[0] < 32))
	{
		if (FuncBeforeEditChanged)
			FuncBeforeEditChanged();

		DeleteSelectedText();
		ClearSelection();
		if (input_mask.empty())
		{
			// not in any special mode, just insert the string.
			InsertText(cursor_pos, chars);
			cursor_pos.x += chars.size();
		}
		else
		{
			if (InputMaskAcceptsInput(cursor_pos, chars))
			{
				InsertText(cursor_pos, chars);
				cursor_pos.x += chars.size();
			}
		}

		if (FuncAfterEditChanged)
			FuncAfterEditChanged();
	}
}

void TextEdit::OnKeyDown(EInputKey key)
{
	if (!readonly && key == IK_Enter)
	{
		if (FuncEnterPressed)
		{
			FuncEnterPressed();
		}
		else
		{
			ClearSelection();
			InsertText(cursor_pos, "\n");
			SetCursorPos(GetCursorPos() + 1);
		}
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
		SelectAll();
	}
	else if (key == IK_C && GetKeyState(IK_Ctrl))
	{
		std::string str = GetSelection();
		SetClipboardText(str);
	}
	else if (readonly)
	{
		// Do not consume messages on read only component (only allow CTRL-A and CTRL-C)
		return;
	}
	else if (key == IK_Up)
	{
		if (GetKeyState(IK_Shift) && selection_length == 0)
			selection_start = cursor_pos;

		if (cursor_pos.y > 0)
		{
			cursor_pos.y--;
			cursor_pos.x = std::min(lines[cursor_pos.y].text.size(), (size_t)cursor_pos.x);
		}

		if (GetKeyState(IK_Shift))
		{
			selection_length = ToOffset(cursor_pos) - ToOffset(selection_start);
		}
		else
		{
			// Clear the selection if a cursor key is pressed but shift isn't down. 
			selection_start = ivec2(0, 0);
			selection_length = 0;
		}
		MoveVerticalScroll();
		Update();
		undo_info.first_text_insert = true;
	}
	else if (key == IK_Down)
	{
		if (GetKeyState(IK_Shift) && selection_length == 0)
			selection_start = cursor_pos;

		if (cursor_pos.y < lines.size() - 1)
		{
			cursor_pos.y++;
			cursor_pos.x = std::min(lines[cursor_pos.y].text.size(), (size_t)cursor_pos.x);
		}

		if (GetKeyState(IK_Shift))
		{
			selection_length = ToOffset(cursor_pos) - ToOffset(selection_start);
		}
		else
		{
			// Clear the selection if a cursor key is pressed but shift isn't down. 
			selection_start = ivec2(0, 0);
			selection_length = 0;
		}
		MoveVerticalScroll();

		Update();
		undo_info.first_text_insert = true;
	}
	else if (key == IK_Left)
	{
		Move(-1, GetKeyState(IK_Shift), GetKeyState(IK_Ctrl));
	}
	else if (key == IK_Right)
	{
		Move(1, GetKeyState(IK_Shift), GetKeyState(IK_Ctrl));
	}
	else if (key == IK_Backspace)
	{
		Backspace();
	}
	else if (key == IK_Delete)
	{
		Del();
	}
	else if (key == IK_Home)
	{
		if (GetKeyState(IK_Ctrl))
			cursor_pos = ivec2(0, 0);
		else
			cursor_pos.x = 0;
		if (GetKeyState(IK_Shift))
			selection_length = ToOffset(cursor_pos) - ToOffset(selection_start);
		else
			ClearSelection();
		Update();
		MoveVerticalScroll();
	}
	else if (key == IK_End)
	{
		if (GetKeyState(IK_Ctrl))
			cursor_pos = ivec2(lines.back().text.length(), lines.size() - 1);
		else
			cursor_pos.x = lines[cursor_pos.y].text.size();

		if (GetKeyState(IK_Shift))
			selection_length = ToOffset(cursor_pos) - ToOffset(selection_start);
		else
			ClearSelection();
		Update();
	}
	else if (key == IK_X && GetKeyState(IK_Ctrl))
	{
		std::string str = GetSelection();
		DeleteSelectedText();
		SetClipboardText(str);
	}
	else if (key == IK_V && GetKeyState(IK_Ctrl))
	{
		std::string str = GetClipboardText();
		std::string::const_iterator end_str = std::remove(str.begin(), str.end(), '\r');
		str.resize(end_str - str.begin());
		DeleteSelectedText();

		if (input_mask.empty())
		{
			InsertText(cursor_pos, str);
			SetCursorPos(GetCursorPos() + str.length());
		}
		else
		{
			if (InputMaskAcceptsInput(cursor_pos, str))
			{
				InsertText(cursor_pos, str);
				SetCursorPos(GetCursorPos() + str.length());
			}
		}
		MoveVerticalScroll();
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
		if (selection_length == 0)
			selection_start = cursor_pos;
	}

	if (FuncAfterEditChanged)
		FuncAfterEditChanged();
}

void TextEdit::OnKeyUp(EInputKey key)
{
}

void TextEdit::OnSetFocus()
{
	if (!readonly)
		timer->Start(500);
	if (select_all_on_focus_gain)
		SelectAll();
	ignore_mouse_events = true;
	cursor_pos.y = lines.size() - 1;
	cursor_pos.x = lines[cursor_pos.y].text.length();

	Update();

	if (FuncFocusGained)
		FuncFocusGained();
}

void TextEdit::OnLostFocus()
{
	timer->Stop();
	ClearSelection();

	Update();

	if (FuncFocusLost)
		FuncFocusLost();
}

void TextEdit::CreateComponents()
{
	vert_scrollbar = new Scrollbar(this);
	vert_scrollbar->FuncScroll = [=]() { OnVerticalScroll(); };
	vert_scrollbar->SetVisible(false);
	vert_scrollbar->SetVertical();
}

void TextEdit::OnVerticalScroll()
{
}

void TextEdit::UpdateVerticalScroll()
{
	Rect rect(
		GetWidth() - 16.0/*vert_scrollbar->GetWidth()*/,
		0.0,
		16.0/*vert_scrollbar->GetWidth()*/,
		GetHeight());

	vert_scrollbar->SetFrameGeometry(rect);

	double total_height = GetTotalLineHeight();
	double height_per_line = std::max(1.0, total_height / std::max(1.0, (double)lines.size()));
	bool visible = total_height > GetHeight();
	vert_scrollbar->SetRanges((int)std::round(GetHeight() / height_per_line), (int)std::round(total_height / height_per_line));
	vert_scrollbar->SetLineStep(1);
	vert_scrollbar->SetVisible(visible);

	if (visible == false)
		vert_scrollbar->SetPosition(0);
}

void TextEdit::MoveVerticalScroll()
{
	double total_height = GetTotalLineHeight();
	double height_per_line = std::max(1.0, total_height / std::max((size_t)1, lines.size()));
	int lines_fit = (int)(GetHeight() / height_per_line);
	if (cursor_pos.y >= vert_scrollbar->GetPosition() + lines_fit)
	{
		vert_scrollbar->SetPosition(cursor_pos.y - lines_fit + 1);
	}
	else if (cursor_pos.y < vert_scrollbar->GetPosition())
	{
		vert_scrollbar->SetPosition(cursor_pos.y);
	}
}

double TextEdit::GetTotalLineHeight()
{
	double total = 0;
	for (std::vector<Line>::const_iterator iter = lines.begin(); iter != lines.end(); iter++)
	{
		total += iter->layout.GetSize().height;
	}
	return total;
}

void TextEdit::Move(int steps, bool shift, bool ctrl)
{
	if (shift && selection_length == 0)
		selection_start = cursor_pos;

	// Jump over words if control is pressed.
	if (ctrl)
	{
		if (steps < 0 && cursor_pos.x == 0 && cursor_pos.y > 0)
		{
			cursor_pos.x = (int)lines[cursor_pos.y - 1].text.size();
			cursor_pos.y--;
		}
		else if (steps > 0 && cursor_pos.x == (int)lines[cursor_pos.y].text.size() && cursor_pos.y + 1 < (int)lines.size())
		{
			cursor_pos.x = 0;
			cursor_pos.y++;
		}

		ivec2 new_pos;
		if (steps < 0)
			new_pos = FindPreviousBreakCharacter(cursor_pos);
		else
			new_pos = FindNextBreakCharacter(cursor_pos);

		cursor_pos = new_pos;
	}
	else if (steps < 0 && cursor_pos.x == 0 && cursor_pos.y > 0)
	{
		cursor_pos.x = (int)lines[cursor_pos.y - 1].text.size();
		cursor_pos.y--;
	}
	else if (steps > 0 && cursor_pos.x == (int)lines[cursor_pos.y].text.size() && cursor_pos.y + 1 < (int)lines.size())
	{
		cursor_pos.x = 0;
		cursor_pos.y++;
	}
	else
	{
		UTF8Reader utf8_reader(lines[cursor_pos.y].text.data(), lines[cursor_pos.y].text.length());
		utf8_reader.set_position(cursor_pos.x);
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

		cursor_pos.x = (int)utf8_reader.position();
	}

	if (shift)
	{
		selection_length = (int)ToOffset(cursor_pos) - (int)ToOffset(selection_start);
	}
	else
	{
		// Clear the selection if a cursor key is pressed but shift isn't down. 
		selection_start = ivec2(0, 0);
		selection_length = 0;
	}


	MoveVerticalScroll();
	Update();

	undo_info.first_text_insert = true;
}

std::string TextEdit::break_characters = " ::;,.-";

TextEdit::ivec2 TextEdit::FindNextBreakCharacter(ivec2 search_start)
{
	search_start.x++;
	if (search_start.x >= int(lines[search_start.y].text.size()) - 1)
		return ivec2(lines[search_start.y].text.size(), search_start.y);

	int pos = lines[search_start.y].text.find_first_of(break_characters, search_start.x);
	if (pos == std::string::npos)
		return ivec2(lines[search_start.y].text.size(), search_start.y);
	return ivec2(pos, search_start.y);
}

TextEdit::ivec2 TextEdit::FindPreviousBreakCharacter(ivec2 search_start)
{
	search_start.x--;
	if (search_start.x <= 0)
		return ivec2(0, search_start.y);
	int pos = lines[search_start.y].text.find_last_of(break_characters, search_start.x);
	if (pos == std::string::npos)
		return ivec2(0, search_start.y);
	return ivec2(pos, search_start.y);
}

void TextEdit::InsertText(ivec2 pos, const std::string& str)
{
	undo_info.first_erase = false;
	if (undo_info.first_text_insert)
	{
		undo_info.undo_text = GetText();
		undo_info.first_text_insert = false;
	}

	// checking if insert exceeds max length
	if (ToOffset(ivec2(lines[lines.size() - 1].text.size(), lines.size() - 1)) + str.length() > max_length)
	{
		return;
	}

	std::string::size_type start = 0;
	while (true)
	{
		std::string::size_type next_newline = str.find('\n', start);

		lines[pos.y].text.insert(pos.x, str.substr(start, next_newline - start));
		lines[pos.y].invalidated = true;

		if (next_newline == std::string::npos)
			break;

		pos.x += next_newline - start;

		Line line;
		line.text = lines[pos.y].text.substr(pos.x);
		lines.insert(lines.begin() + pos.y + 1, line);
		lines[pos.y].text = lines[pos.y].text.substr(0, pos.x);
		lines[pos.y].invalidated = true;
		pos = ivec2(0, pos.y + 1);

		start = next_newline + 1;
	}

	MoveVerticalScroll();

	Update();
}

void TextEdit::Backspace()
{
	if (undo_info.first_erase)
	{
		undo_info.first_erase = false;
		undo_info.undo_text = GetText();
	}

	if (GetSelectionLength() != 0)
	{
		DeleteSelectedText();
		ClearSelection();
		Update();
	}
	else
	{
		if (cursor_pos.x > 0)
		{
			UTF8Reader utf8_reader(lines[cursor_pos.y].text.data(), lines[cursor_pos.y].text.length());
			utf8_reader.set_position(cursor_pos.x);
			utf8_reader.prev();
			int length = utf8_reader.char_length();
			lines[cursor_pos.y].text.erase(cursor_pos.x - length, length);
			lines[cursor_pos.y].invalidated = true;
			cursor_pos.x -= length;
			Update();
		}
		else if (cursor_pos.y > 0)
		{
			selection_start = ivec2(lines[cursor_pos.y - 1].text.length(), cursor_pos.y - 1);
			selection_length = 1;
			DeleteSelectedText();
		}
	}
	MoveVerticalScroll();
}

void TextEdit::Del()
{
	if (undo_info.first_erase)
	{
		undo_info.first_erase = false;
		undo_info.undo_text = GetText();
	}

	if (GetSelectionLength() != 0)
	{
		DeleteSelectedText();
		ClearSelection();
		Update();
	}
	else
	{
		if (cursor_pos.x < (int)lines[cursor_pos.y].text.size())
		{
			UTF8Reader utf8_reader(lines[cursor_pos.y].text.data(), lines[cursor_pos.y].text.length());
			utf8_reader.set_position(cursor_pos.x);
			int length = utf8_reader.char_length();
			lines[cursor_pos.y].text.erase(cursor_pos.x, length);
			lines[cursor_pos.y].invalidated = true;
			Update();
		}
		else if (cursor_pos.y + 1 < lines.size())
		{
			selection_start = ivec2(lines[cursor_pos.y].text.length(), cursor_pos.y);
			selection_length = 1;
			DeleteSelectedText();
		}
	}
	MoveVerticalScroll();
}

void TextEdit::OnTimerExpired()
{
	if (IsVisible() == false)
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

void TextEdit::OnGeometryChanged()
{
	Canvas* canvas = GetCanvas();

	vertical_text_align = canvas->verticalTextAlign();

	clip_start_offset = 0;
	UpdateVerticalScroll();
}

void TextEdit::OnScrollTimerExpired()
{
	if (mouse_moves_left)
		Move(-1, false, false);
	else
		Move(1, false, false);
}

void TextEdit::OnEnableChanged()
{
	bool enabled = IsEnabled();
	if (!enabled)
	{
		cursor_blink_visible = false;
		timer->Stop();
	}
	Update();
}

bool TextEdit::InputMaskAcceptsInput(ivec2 cursor_pos, const std::string& str)
{
	return str.find_first_not_of(input_mask) == std::string::npos;
}

std::string::size_type TextEdit::ToOffset(ivec2 pos) const
{
	if (pos.y < lines.size())
	{
		std::string::size_type offset = 0;
		for (int line = 0; line < pos.y; line++)
		{
			offset += lines[line].text.size() + 1;
		}
		return offset + std::min((size_t)pos.x, lines[pos.y].text.size());
	}
	else
	{
		std::string::size_type offset = 0;
		for (size_t line = 0; line < lines.size(); line++)
		{
			offset += lines[line].text.size() + 1;
		}
		return offset - 1;
	}
}

TextEdit::ivec2 TextEdit::FromOffset(std::string::size_type offset) const
{
	int line_offset = 0;
	for (int line = 0; line < lines.size(); line++)
	{
		if (offset <= line_offset + lines[line].text.size())
		{
			return ivec2(offset - line_offset, line);
		}
		line_offset += lines[line].text.size() + 1;
	}
	return ivec2(lines.back().text.size(), lines.size() - 1);
}

double TextEdit::GetTotalHeight()
{
	Canvas* canvas = GetCanvas();
	LayoutLines(canvas);
	if (!lines.empty())
	{
		return lines.back().box.bottom();
	}
	else
	{
		return GetHeight();
	}
}

void TextEdit::LayoutLines(Canvas* canvas)
{
	ivec2 sel_start;
	ivec2 sel_end;
	if (selection_length > 0)
	{
		sel_start = selection_start;
		sel_end = FromOffset(ToOffset(selection_start) + selection_length);
	}
	else if (selection_length < 0)
	{
		sel_start = FromOffset(ToOffset(selection_start) + selection_length);
		sel_end = selection_start;
	}

	Point draw_pos;
	for (size_t i = vert_scrollbar->GetPosition(); i < lines.size(); i++)
	{
		Line& line = lines[i];
		if (line.invalidated)
		{
			line.layout.Clear();
			if (!line.text.empty())
				line.layout.AddText(line.text, font, Colorf::fromRgba8(255, 255, 255));
			else
				line.layout.AddText(" ", font, Colorf::fromRgba8(255, 255, 255)); // Draw one space character to get the correct height
			line.layout.Layout(canvas, GetWidth());
			line.box = Rect(draw_pos, line.layout.GetSize());
			line.invalidated = false;
		}

		if (sel_start != sel_end && sel_start.y <= i && sel_end.y >= i)
		{
			line.layout.SetSelectionRange(sel_start.y < i ? 0 : sel_start.x, sel_end.y > i ? line.text.size() : sel_end.x);
		}
		else
		{
			line.layout.SetSelectionRange(0, 0);
		}

		line.layout.HideCursor();
		if (HasFocus())
		{
			if (cursor_blink_visible && cursor_pos.y == i)
			{
				line.layout.SetCursorPos(cursor_pos.x);
				line.layout.SetCursorColor(Colorf::fromRgba8(255, 255, 255));
				line.layout.ShowCursor();
			}
		}

		line.box.x = draw_pos.x;
		line.box.y = draw_pos.y;
		line.layout.SetPosition(line.box.topLeft());

		draw_pos = line.box.bottomLeft();
	}
	UpdateVerticalScroll();
}

void TextEdit::OnPaintFrame(Canvas* canvas)
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

void TextEdit::OnPaint(Canvas* canvas)
{
	LayoutLines(canvas);
	for (size_t i = vert_scrollbar->GetPosition(); i < lines.size(); i++)
		lines[i].layout.DrawLayout(canvas);
}

TextEdit::ivec2 TextEdit::GetCharacterIndex(Point mouse_wincoords)
{
	Canvas* canvas = GetCanvas();
	for (size_t i = 0; i < lines.size(); i++)
	{
		Line& line = lines[i];
		if (line.box.top() <= mouse_wincoords.y && line.box.bottom() > mouse_wincoords.y)
		{
			SpanLayout::HitTestResult result = line.layout.HitTest(canvas, mouse_wincoords);
			switch (result.type)
			{
			case SpanLayout::HitTestResult::inside:
				return ivec2(clamp(result.offset, (size_t)0, line.text.size()), i);
			case SpanLayout::HitTestResult::outside_left:
				return ivec2(0, i);
			case SpanLayout::HitTestResult::outside_right:
				return ivec2(line.text.size(), i);
			}
		}
	}

	return ivec2(lines.back().text.size(), lines.size() - 1);
}
