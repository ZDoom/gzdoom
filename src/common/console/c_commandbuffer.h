#pragma once
#include <string>
#include "zstring.h"

struct FCommandBuffer
{
private:
	std::u32string Text;
	unsigned CursorPos = 0;
	unsigned StartPos = 0;	// First character to display
	unsigned CursorPosCells = 0;
	unsigned StartPosCells = 0;

	std::u32string YankBuffer;	// Deleted text buffer

public:
	bool AppendToYankBuffer = false;	// Append consecutive deletes to buffer
	int ConCols;

	FCommandBuffer() = default;

	FCommandBuffer(const FCommandBuffer &o);
	FString GetText() const;

	size_t TextLength() const
	{
		return Text.length();
	}

	void Draw(int x, int y, int scale, bool cursor);
	unsigned CalcCellSize(unsigned length);
	unsigned CharsForCells(unsigned cellin, bool *overflow);
	void MakeStartPosGood();
	void CursorStart();
	void CursorEnd();

private:
	void MoveCursorLeft()
	{
		CursorPos--;
	}

	void MoveCursorRight()
	{
		CursorPos++;
	}

public:
	void CursorLeft();
	void CursorRight();
	void CursorWordLeft();
	void CursorWordRight();
	void DeleteLeft();
	void DeleteRight();
	void DeleteWordLeft();
	void DeleteLineLeft();
	void DeleteLineRight();
	void AddChar(int character);
	void AddString(FString clip);
	void SetString(const FString &str);
	void AddYankBuffer();
};

extern FCommandBuffer CmdLine;
