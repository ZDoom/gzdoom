/*
** c_notifybufferbase.cpp
** Implements the buffer for the notification message
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** Copyright 2005-2020 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "v_text.h"
#include "i_time.h"
#include "v_draw.h"
#include "c_notifybufferbase.h"


void FNotifyBufferBase::Shift(int maxlines)
{
	if (maxlines >= 0 && Text.Size() > (unsigned)maxlines)
	{
		Text.Delete(0, Text.Size() - maxlines);
	}
}

void FNotifyBufferBase::Clear() 
{ 
	Text.Clear(); 
}


void FNotifyBufferBase::AddString(int printlevel, FFont *printFont, const FString &source, int formatwidth, float keeptime, int maxlines)
{
	if (printFont == nullptr) return;	// Without an initialized font we cannot handle the message (this is for those which come here before the font system is ready.)
	LineHeight = printFont->GetHeight();
	TArray<FBrokenLines> lines;

	if (AddType == APPENDLINE && Text.Size() > 0 && Text[Text.Size() - 1].PrintLevel == printlevel)
	{
		FString str = Text[Text.Size() - 1].Text + source;
		lines = V_BreakLines (printFont, formatwidth, str);
	}
	else
	{
		lines = V_BreakLines (printFont, formatwidth, source);
		if (AddType == APPENDLINE)
		{
			AddType = NEWLINE;
		}
	}

	if (lines.Size() == 0)
		return;

	for (auto &line : lines)
	{
		FNotifyText newline;

		newline.Text = line.Text;
		newline.TimeOut = int(keeptime * GameTicRate);
		newline.Ticker = 0;
		newline.PrintLevel = printlevel;
		if (AddType == NEWLINE || Text.Size() == 0)
		{
			if (maxlines > 0)
			{
				Shift(maxlines - 1);
			}
			Text.Push(newline);
		}
		else
		{
			Text[Text.Size() - 1] = newline;
		}
		AddType = NEWLINE;
	}

	switch (source[source.Len()-1])
	{
	case '\r':	AddType = REPLACELINE;	break;
	case '\n':	AddType = NEWLINE;		break;
	default:	AddType = APPENDLINE;	break;
	}

	TopGoal = 0;
}

void FNotifyBufferBase::Tick()
{
	if (TopGoal > Top)
	{
		Top++;
	}
	else if (TopGoal < Top)
	{
		Top--;
	}

	// Remove lines from the beginning that have expired.
	unsigned i;
	for (i = 0; i < Text.Size(); ++i)
	{
		Text[i].Ticker++;
	}

	for (i = 0; i < Text.Size(); ++i)
	{
		if (Text[i].TimeOut != 0 && Text[i].TimeOut > Text[i].Ticker)
			break;
	}
	if (i > 0)
	{
		Text.Delete(0, i);
		Top += LineHeight;
	}
}

