/*
** zstring.cpp
** A dynamically-allocated string class.
**
**---------------------------------------------------------------------------
** Copyright 2005-2008 Randy Heit
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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <new>		// for bad_alloc

#include "zstring.h"

FNullStringData FString::NullString =
{
	0,			// Length of string
	2,			// Size of character buffer
	2,			// RefCount; it must never be modified, so keep it above 1 user at all times
	"\0"
};

void FString::AttachToOther (const FString &other)
{
	assert (other.Chars != NULL);

	if (other.Data()->RefCount < 0)
	{
		AllocBuffer (other.Data()->Len);
		StrCopy (Chars, other.Chars, other.Data()->Len);
	}
	else
	{
		Chars = const_cast<FString &>(other).Data()->AddRef();
	}
}

FString::FString (const char *copyStr)
{
	if (copyStr == NULL || *copyStr == '\0')
	{
		ResetToNull();
	}
	else
	{
		size_t len = strlen (copyStr);
		AllocBuffer (len);
		StrCopy (Chars, copyStr, len);
	}
}

FString::FString (const char *copyStr, size_t len)
{
	AllocBuffer (len);
	StrCopy (Chars, copyStr, len);
}

FString::FString (char oneChar)
{
	if (oneChar == '\0')
	{
		ResetToNull();
	}
	else
	{
		AllocBuffer (1);
		Chars[0] = oneChar;
		Chars[1] = '\0';
	}
}

FString::FString (const FString &head, const FString &tail)
{
	size_t len1 = head.Len();
	size_t len2 = tail.Len();
	AllocBuffer (len1 + len2);
	StrCopy (Chars, head);
	StrCopy (Chars + len1, tail);
}

FString::FString (const FString &head, const char *tail)
{
	size_t len1 = head.Len();
	size_t len2 = strlen (tail);
	AllocBuffer (len1 + len2);
	StrCopy (Chars, head);
	StrCopy (Chars + len1, tail, len2);
}

FString::FString (const FString &head, char tail)
{
	size_t len1 = head.Len();
	AllocBuffer (len1 + 1);
	StrCopy (Chars, head);
	Chars[len1] = tail;
	Chars[len1+1] = '\0';
}

FString::FString (const char *head, const FString &tail)
{
	size_t len1 = strlen (head);
	size_t len2 = tail.Len();
	AllocBuffer (len1 + len2);
	StrCopy (Chars, head, len1);
	StrCopy (Chars + len1, tail);
}

FString::FString (const char *head, const char *tail)
{
	size_t len1 = strlen (head);
	size_t len2 = strlen (tail);
	AllocBuffer (len1 + len2);
	StrCopy (Chars, head, len1);
	StrCopy (Chars + len1, tail, len2);
}

FString::FString (char head, const FString &tail)
{
	size_t len2 = tail.Len();
	AllocBuffer (1 + len2);
	Chars[0] = head;
	StrCopy (Chars + 1, tail);
}

FString::~FString ()
{
	Data()->Release();
}

char *FString::LockNewBuffer(size_t len)
{
	Data()->Release();
	AllocBuffer(len);
	assert(Data()->RefCount == 1);
	Data()->RefCount = -1;
	return Chars;
}

char *FString::LockBuffer()
{
	if (Data()->RefCount == 1)
	{ // We're the only user, so we can lock it straight away
		Data()->RefCount = -1;
	}
	else if (Data()->RefCount < -1)
	{ // Already locked; just add to the lock count
		Data()->RefCount--;
	}
	else
	{ // Somebody else is also using this character buffer, so create a copy
		FStringData *old = Data();
		AllocBuffer (old->Len);
		StrCopy (Chars, old->Chars(), old->Len);
		old->Release();
		Data()->RefCount = -1;
	}
	return Chars;
}

void FString::UnlockBuffer()
{
	assert (Data()->RefCount < 0);

	if (++Data()->RefCount == 0)
	{
		Data()->RefCount = 1;
	}
}

FString &FString::operator = (const FString &other)
{
	assert (Chars != NULL);

	if (&other != this)
	{
		int oldrefcount = Data()->RefCount < 0;
		Data()->Release();
		AttachToOther(other);
		if (oldrefcount < 0)
		{
			LockBuffer();
			Data()->RefCount = oldrefcount;
		}
	}
	return *this;
}

FString &FString::operator = (FString &&other)
{
	assert (Chars != NULL);

	if (&other != this)
	{
		Data()->Release();
		Chars = other.Chars;
		other.ResetToNull();
	}

	return *this;
}

FString &FString::operator = (const char *copyStr)
{
	if (copyStr != Chars)
	{
		if (copyStr == NULL || *copyStr == '\0')
		{
			Data()->Release();
			ResetToNull();
		}
		else
		{
			// In case copyStr is inside us, we can't release it until
			// we've finished the copy.
			FStringData *old = Data();

			if (copyStr < Chars || copyStr >= Chars + old->Len)
			{
				// We know the string isn't in our buffer, so release it now
				// to reduce the potential for needless memory fragmentation.
				old->Release();
				old = NULL;
			}
			size_t len = strlen (copyStr);
			AllocBuffer (len);
			StrCopy (Chars, copyStr, len);
			if (old != NULL)
			{
				old->Release();
			}
		}
	}
	return *this;
}

void FString::Format (const char *fmt, ...)
{
	va_list arglist;
	va_start (arglist, fmt);
	VFormat (fmt, arglist);
	va_end (arglist);
}

void FString::AppendFormat (const char *fmt, ...)
{
	va_list arglist;
	va_start (arglist, fmt);
	StringFormat::VWorker (FormatHelper, this, fmt, arglist);
	va_end (arglist);
}

void FString::VFormat (const char *fmt, va_list arglist)
{
	Data()->Release();
	Chars = (char *)(FStringData::Alloc(128) + 1);
	StringFormat::VWorker (FormatHelper, this, fmt, arglist);
}

void FString::VAppendFormat (const char *fmt, va_list arglist)
{
	StringFormat::VWorker (FormatHelper, this, fmt, arglist);
}

int FString::FormatHelper (void *data, const char *cstr, int len)
{
	FString *str = (FString *)data;
	size_t len1 = str->Len();
	if (len1 + len > str->Data()->AllocLen || str->Chars == &NullString.Nothing[0])
	{
		str->ReallocBuffer((len1 + len + 127) & ~127);
	}
	StrCopy (str->Chars + len1, cstr, len);
	str->Data()->Len = (unsigned int)(len1 + len);
	return len;
}

FString FString::operator + (const FString &tail) const
{
	return FString (*this, tail);
}

FString FString::operator + (const char *tail) const
{
	return FString (*this, tail);
}

FString operator + (const char *head, const FString &tail)
{
	return FString (head, tail);
}

FString FString::operator + (char tail) const
{
	return FString (*this, tail);
}

FString operator + (char head, const FString &tail)
{
	return FString (head, tail);
}

FString &FString::operator += (const FString &tail)
{
	size_t len1 = Len();
	size_t len2 = tail.Len();
	ReallocBuffer (len1 + len2);
	StrCopy (Chars + len1, tail);
	return *this;
}

FString &FString::operator += (const char *tail)
{
	size_t len1 = Len();
	size_t len2 = strlen(tail);
	ReallocBuffer (len1 + len2);
	StrCopy (Chars + len1, tail, len2);
	return *this;
}

FString &FString::operator += (char tail)
{
	size_t len1 = Len();
	ReallocBuffer (len1 + 1);
	Chars[len1] = tail;
	Chars[len1+1] = '\0';
	return *this;
}

FString &FString::AppendCStrPart (const char *tail, size_t tailLen)
{
	if (tailLen > 0)
	{
		size_t len1 = Len();
		ReallocBuffer(len1 + tailLen);
		StrCopy(Chars + len1, tail, tailLen);
	}
	return *this;
}

FString &FString::CopyCStrPart(const char *tail, size_t tailLen)
{
	if (tailLen > 0)
	{
		ReallocBuffer(tailLen);
		StrCopy(Chars, tail, tailLen);
	}
	else
	{
		Data()->Release();
		ResetToNull();
	}
	return *this;
}

void FString::Truncate(size_t newlen)
{
	if (newlen == 0)
	{
		Data()->Release();
		ResetToNull();
	}
	else if (newlen < Len())
	{
		ReallocBuffer (newlen);
		Chars[newlen] = '\0';
	}
}

void FString::Remove(size_t index, size_t remlen)
{
	if (index < Len())
	{
		if (index + remlen >= Len())
		{
			Truncate((long)index);
		}
		else
		{
			if (Data()->RefCount == 1)
			{ // Can do this in place
				memmove(Chars + index, Chars + index + remlen, Len() - index - remlen);
				memset(Chars + Len() - remlen, 0, remlen);
				Data()->Len -= (unsigned)remlen;
			}
			else
			{ // Must do it in a copy
				FStringData *old = Data();
				AllocBuffer(old->Len - remlen);
				StrCopy(Chars, old->Chars(), index);
				StrCopy(Chars + index, old->Chars() + index + remlen, old->Len - index - remlen);
				old->Release();
			}
		}
	}
}

FString FString::Left (size_t numChars) const
{
	size_t len = Len();
	if (len < numChars)
	{
		numChars = len;
	}
	return FString (Chars, numChars);
}

FString FString::Right (size_t numChars) const
{
	size_t len = Len();
	if (len < numChars)
	{
		numChars = len;
	}
	return FString (Chars + len - numChars, numChars);
}

FString FString::Mid (size_t pos, size_t numChars) const
{
	size_t len = Len();
	if (pos >= len)
	{
		return FString();
	}
	if (pos + numChars > len || pos + numChars < pos)
	{
		numChars = len - pos;
	}
	return FString (Chars + pos, numChars);
}

long FString::IndexOf (const FString &substr, long startIndex) const
{
	return IndexOf (substr.Chars, startIndex);
}

long FString::IndexOf (const char *substr, long startIndex) const
{
	if (startIndex > 0 && Len() <= (size_t)startIndex)
	{
		return -1;
	}
	char *str = strstr (Chars + startIndex, substr);
	if (str == NULL)
	{
		return -1;
	}
	return long(str - Chars);
}

long FString::IndexOf (char subchar, long startIndex) const
{
	if (startIndex > 0 && Len() <= (size_t)startIndex)
	{
		return -1;
	}
	char *str = strchr (Chars + startIndex, subchar);
	if (str == NULL)
	{
		return -1;
	}
	return long(str - Chars);
}

long FString::IndexOfAny (const FString &charset, long startIndex) const
{
	return IndexOfAny (charset.Chars, startIndex);
}

long FString::IndexOfAny (const char *charset, long startIndex) const
{
	if (startIndex > 0 && Len() <= (size_t)startIndex)
	{
		return -1;
	}
	char *brk = strpbrk (Chars + startIndex, charset);
	if (brk == NULL)
	{
		return -1;
	}
	return long(brk - Chars);
}

long FString::LastIndexOf (const FString &substr) const
{
	return LastIndexOf (substr.Chars, long(Len()), substr.Len());
}

long FString::LastIndexOf (const char *substr) const
{
	return LastIndexOf (substr, long(Len()), strlen(substr));
}

long FString::LastIndexOf (char subchar) const
{
	return LastIndexOf (subchar, long(Len()));
}

long FString::LastIndexOf (const FString &substr, long endIndex) const
{
	return LastIndexOf (substr.Chars, endIndex, substr.Len());
}

long FString::LastIndexOf (const char *substr, long endIndex) const
{
	return LastIndexOf (substr, endIndex, strlen(substr));
}

long FString::LastIndexOf (char subchar, long endIndex) const
{
	if ((size_t)endIndex > Len())
	{
		endIndex = long(Len());
	}
	while (--endIndex >= 0)
	{
		if (Chars[endIndex] == subchar)
		{
			return endIndex;
		}
	}
	return -1;
}

long FString::LastIndexOf (const char *substr, long endIndex, size_t substrlen) const
{
	if ((size_t)endIndex > Len())
	{
		endIndex = long(Len());
	}
	substrlen--;
	while (--endIndex >= long(substrlen))
	{
		if (strncmp (substr, Chars + endIndex - substrlen, substrlen + 1) == 0)
		{
			return endIndex;
		}
	}
	return -1;
}

long FString::LastIndexOfAny (const FString &charset) const
{
	return LastIndexOfAny (charset.Chars, long(Len()));
}

long FString::LastIndexOfAny (const char *charset) const
{
	return LastIndexOfAny (charset, long(Len()));
}

long FString::LastIndexOfAny (const FString &charset, long endIndex) const
{
	return LastIndexOfAny (charset.Chars, endIndex);
}

long FString::LastIndexOfAny (const char *charset, long endIndex) const
{
	if ((size_t)endIndex > Len())
	{
		endIndex = long(Len());
	}
	while (--endIndex >= 0)
	{
		if (strchr (charset, Chars[endIndex]) != NULL)
		{
			return endIndex;
		}
	}
	return -1;
}

void FString::ToUpper ()
{
	LockBuffer();
	size_t max = Len();
	for (size_t i = 0; i < max; ++i)
	{
		Chars[i] = (char)toupper(Chars[i]);
	}
	UnlockBuffer();
}

void FString::ToLower ()
{
	LockBuffer();
	size_t max = Len();
	for (size_t i = 0; i < max; ++i)
	{
		Chars[i] = (char)tolower(Chars[i]);
	}
	UnlockBuffer();
}

void FString::SwapCase ()
{
	LockBuffer();
	size_t max = Len();
	for (size_t i = 0; i < max; ++i)
	{
		if (isupper(Chars[i]))
		{
			Chars[i] = (char)tolower(Chars[i]);
		}
		else
		{
			Chars[i] = (char)toupper(Chars[i]);
		}
	}
	UnlockBuffer();
}

void FString::StripLeft ()
{
	size_t max = Len(), i, j;
	if (max == 0) return;
	for (i = 0; i < max; ++i)
	{
		if (!isspace((unsigned char)Chars[i]))
			break;
	}
	if (i == 0)
	{ // Nothing to strip.
		return;
	}
	if (Data()->RefCount <= 1)
	{
		for (j = 0; i <= max; ++j, ++i)
		{
			Chars[j] = Chars[i];
		}
		ReallocBuffer (j-1);
	}
	else
	{
		FStringData *old = Data();
		AllocBuffer (max - i);
		StrCopy (Chars, old->Chars() + i, max - i);
		old->Release();
	}
}

void FString::StripLeft (const FString &charset)
{
	return StripLeft (charset.Chars);
}

void FString::StripLeft (const char *charset)
{
	size_t max = Len(), i, j;
	if (max == 0) return;
	for (i = 0; i < max; ++i)
	{
		if (!strchr (charset, Chars[i]))
			break;
	}
	if (i == 0)
	{ // Nothing to strip.
		return;
	}
	if (Data()->RefCount <= 1)
	{
		for (j = 0; i <= max; ++j, ++i)
		{
			Chars[j] = Chars[i];
		}
		ReallocBuffer (j-1);
	}
	else
	{
		FStringData *old = Data();
		AllocBuffer (max - i);
		StrCopy (Chars, old->Chars() + i, max - i);
		old->Release();
	}
}

void FString::StripRight ()
{
	size_t max = Len(), i;
	if (max == 0) return;
	for (i = --max; i > 0; i--)
	{
		if (!isspace((unsigned char)Chars[i]))
			break;
	}
	if (i == max)
	{ // Nothing to strip.
		return;
	}
	if (Data()->RefCount <= 1)
	{
		Chars[i+1] = '\0';
		ReallocBuffer (i+1);
	}
	else
	{
		FStringData *old = Data();
		AllocBuffer (i+1);
		StrCopy (Chars, old->Chars(), i+1);
		old->Release();
	}
}

void FString::StripRight (const FString &charset)
{
	return StripRight (charset.Chars);
}

void FString::StripRight (const char *charset)
{
	size_t max = Len(), i;
	if (max == 0) return;
	for (i = --max; i > 0; i--)
	{
		if (!strchr (charset, Chars[i]))
			break;
	}
	if (i == max)
	{ // Nothing to strip.
		return;
	}
	if (Data()->RefCount <= 1)
	{
		Chars[i+1] = '\0';
		ReallocBuffer (i+1);
	}
	else
	{
		FStringData *old = Data();
		AllocBuffer (i+1);
		StrCopy (Chars, old->Chars(), i+1);
		old->Release();
	}
}

void FString::StripLeftRight ()
{
	size_t max = Len(), i, j, k;
	if (max == 0) return;
	for (i = 0; i < max; ++i)
	{
		if (!isspace((unsigned char)Chars[i]))
			break;
	}
	for (j = max - 1; j >= i; --j)
	{
		if (!isspace((unsigned char)Chars[j]))
			break;
	}
	if (i == 0 && j == max - 1)
	{ // Nothing to strip.
		return;
	}
	if (Data()->RefCount <= 1)
	{
		for (k = 0; i <= j; ++i, ++k)
		{
			Chars[k] = Chars[i];
		}
		Chars[k] = '\0';
		ReallocBuffer (k);
	}
	else
	{
		FStringData *old = Data();
		AllocBuffer(j - i + 1);
		StrCopy(Chars, old->Chars(), j - i + 1);
		old->Release();
	}
}

void FString::StripLeftRight (const FString &charset)
{
	return StripLeftRight (charset.Chars);
}

void FString::StripLeftRight (const char *charset)
{
	size_t max = Len(), i, j, k;
	if (max == 0) return;
	for (i = 0; i < max; ++i)
	{
		if (!strchr (charset, Chars[i]))
			break;
	}
	for (j = max - 1; j >= i; --j)
	{
		if (!strchr (charset, Chars[j]))
			break;
	}
	if (Data()->RefCount <= 1)
	{
		for (k = 0; i <= j; ++i, ++k)
		{
			Chars[k] = Chars[i];
		}
		Chars[k] = '\0';
		ReallocBuffer (k);
	}
	else
	{
		FStringData *old = Data();
		AllocBuffer (j - i);
		StrCopy (Chars, old->Chars(), j - i);
		old->Release();
	}
}

void FString::Insert (size_t index, const FString &instr)
{
	Insert (index, instr.Chars, instr.Len());
}

void FString::Insert (size_t index, const char *instr)
{
	Insert (index, instr, strlen(instr));
}

void FString::Insert (size_t index, const char *instr, size_t instrlen)
{
	if (instrlen > 0)
	{
		size_t mylen = Len();
		if (index >= mylen)
		{
			AppendCStrPart(instr, instrlen);
		}
		else if (Data()->RefCount <= 1)
		{
			ReallocBuffer(mylen + instrlen);
			memmove(Chars + index + instrlen, Chars + index, (mylen - index + 1) * sizeof(char));
			memcpy(Chars + index, instr, instrlen * sizeof(char));
		}
		else
		{
			FStringData *old = Data();
			AllocBuffer(mylen + instrlen);
			StrCopy(Chars, old->Chars(), index);
			StrCopy(Chars + index, instr, instrlen);
			StrCopy(Chars + index + instrlen, old->Chars() + index, mylen - index);
			old->Release();
		}
	}
}

void FString::ReplaceChars (char oldchar, char newchar)
{
	if (oldchar == '\0')
		return;

	ReplaceChars([&oldchar](char c){ return c == oldchar; }, newchar);
}

void FString::ReplaceChars (const char *oldcharset, char newchar)
{
	if (oldcharset == NULL || oldcharset[0] == '\0')
		return;

	ReplaceChars([&oldcharset](char c){ return strchr(oldcharset, c) != NULL; }, newchar);
}

void FString::StripChars (char killchar)
{
	if (killchar == '\0')
		return;

	StripChars([&killchar](char c){ return c == killchar; });
}

void FString::StripChars (const char *killcharset)
{
	if (killcharset == NULL || killcharset[0] == '\0')
		return;

	StripChars([&killcharset](char c){ return strchr(killcharset, c) != NULL; });
}

void FString::MergeChars (char merger)
{
	MergeChars (merger, merger);
}

void FString::MergeChars (char merger, char newchar)
{
	size_t read, write, mylen;

	LockBuffer();
	for (read = write = 0, mylen = Len(); read < mylen; )
	{
		if (Chars[read] == merger)
		{
			while (Chars[++read] == merger)
			{
			}
			Chars[write++] = newchar;
		}
		else
		{
			Chars[write++] = Chars[read++];
		}
	}
	Chars[write] = '\0';
	ReallocBuffer (write);
	UnlockBuffer();
}

void FString::MergeChars (const char *charset, char newchar)
{
	size_t read, write, mylen;

	LockBuffer();
	for (read = write = 0, mylen = Len(); read < mylen; )
	{
		if (strchr (charset, Chars[read]) != NULL)
		{
			while (strchr (charset, Chars[++read]) != NULL)
			{
			}
			Chars[write++] = newchar;
		}
		else
		{
			Chars[write++] = Chars[read++];
		}
	}
	Chars[write] = '\0';
	ReallocBuffer (write);
	UnlockBuffer();
}

void FString::Substitute (const FString &oldstr, const FString &newstr)
{
	return Substitute (oldstr.Chars, newstr.Chars, oldstr.Len(), newstr.Len());
}

void FString::Substitute (const char *oldstr, const FString &newstr)
{
	return Substitute (oldstr, newstr.Chars, strlen(oldstr), newstr.Len());
}

void FString::Substitute (const FString &oldstr, const char *newstr)
{
	return Substitute (oldstr.Chars, newstr, oldstr.Len(), strlen(newstr));
}

void FString::Substitute (const char *oldstr, const char *newstr)
{
	return Substitute (oldstr, newstr, strlen(oldstr), strlen(newstr));
}

void FString::Substitute (const char *oldstr, const char *newstr, size_t oldstrlen, size_t newstrlen)
{
	LockBuffer();
	for (size_t checkpt = 0; checkpt < Len(); )
	{
		char *match = strstr (Chars + checkpt, oldstr);
		size_t len = Len();
		if (match != NULL)
		{
			size_t matchpt = match - Chars;
			if (oldstrlen != newstrlen)
			{
				ReallocBuffer (len + newstrlen - oldstrlen);
				memmove (Chars + matchpt + newstrlen, Chars + matchpt + oldstrlen, (len + 1 - matchpt - oldstrlen)*sizeof(char));
			}
			memcpy (Chars + matchpt, newstr, newstrlen);
			checkpt = matchpt + newstrlen;
		}
		else
		{
			break;
		}
	}
	UnlockBuffer();
}

bool FString::IsInt () const
{
	// String must match: [whitespace] [{+ | –}] [0 [{ x | X }]] [digits] [whitespace]

/* This state machine is based on a simplification of re2c's output for this input:
digits		= [0-9];
hexdigits	= [0-9a-fA-F];
octdigits	= [0-7];

("0" octdigits+ | "0" [xX] hexdigits+ | (digits \ '0') digits*) { return true; }
[\000-\377] { return false; }*/
	const char *YYCURSOR = Chars;
	char yych;

	yych = *YYCURSOR;

	// Skip preceding whitespace
	while (yych != '\0' && isspace((unsigned char)yych)) { yych = *++YYCURSOR; }

	// Check for sign
	if (yych == '+' || yych == '-') { yych = *++YYCURSOR; }

	if (yych == '0')
	{
		yych = *++YYCURSOR;
		if (yych >= '0' && yych <= '7')
		{
			do { yych = *++YYCURSOR; } while (yych >= '0' && yych <= '7');
		}
		else if (yych == 'X' || yych == 'x')
		{
			bool gothex = false;
			yych = *++YYCURSOR;
			while ((yych >= '0' && yych <= '9') || (yych >= 'A' && yych <= 'F') || (yych >= 'a' && yych <= 'f'))
			{
				gothex = true;
				yych = *++YYCURSOR;
			}
			if (!gothex) return false;
		}
		else
		{
			return false;
		}
	}
	else if (yych >= '1' && yych <= '9')
	{
		do { yych = *++YYCURSOR; } while (yych >= '0' && yych <= '9');
	}
	else
	{
		return false;
	}

	// The rest should all be whitespace
	while (yych != '\0' && isspace((unsigned char)yych)) { yych = *++YYCURSOR; }
	return yych == '\0';
}

bool FString::IsFloat () const
{
	// String must match: [whitespace] [sign] [digits] [.digits] [ {d | D | e | E}[sign]digits] [whitespace]
/* This state machine is based on a simplification of re2c's output for this input:
digits		= [0-9];

(digits+ | digits* "." digits+) ([dDeE] [+-]? digits+)? { return true; }
[\000-\377] { return false; }
*/
	const char *YYCURSOR = Chars;
	char yych;
	bool gotdig = false;

	yych = *YYCURSOR;

	// Skip preceding whitespace
	while (yych != '\0' && isspace((unsigned char)yych)) { yych = *++YYCURSOR; }

	// Check for sign
	if (yych == '+' || yych == '-') { yych = *++YYCURSOR; }

	while (yych >= '0' && yych <= '9')
	{
		gotdig = true;
		yych = *++YYCURSOR;
	}
	if (yych == '.')
	{
		yych = *++YYCURSOR;
		if (yych >= '0' && yych <= '9')
		{
			gotdig = true;
			do { yych = *++YYCURSOR; } while (yych >= '0' && yych <= '9');
		}
		else return false;
	}
	if (gotdig)
	{
		if (yych == 'D' || yych == 'd' || yych == 'E' || yych == 'e')
		{
			yych = *++YYCURSOR;
			if (yych == '+' || yych == '-') yych = *++YYCURSOR;
			while (yych >= '0' && yych <= '9') { yych = *++YYCURSOR; }
		}
	}

	// The rest should all be whitespace
	while (yych != '\0' && isspace((unsigned char)yych)) { yych = *++YYCURSOR; }
	return yych == '\0';
}

long FString::ToLong (int base) const
{
	return (long)strtoll (Chars, NULL, base);
}

unsigned long FString::ToULong (int base) const
{
	return (unsigned long)strtoull (Chars, NULL, base);
}

double FString::ToDouble () const
{
	return strtod (Chars, NULL);
}

void FString::StrCopy (char *to, const char *from, size_t len)
{
	memcpy (to, from, len*sizeof(char));
	to[len] = 0;
}

void FString::StrCopy (char *to, const FString &from)
{
	StrCopy (to, from.Chars, from.Len());
}

void FString::AllocBuffer (size_t len)
{
	Chars = (char *)(FStringData::Alloc(len) + 1);
	Data()->Len = (unsigned int)len;
}

void FString::ReallocBuffer (size_t newlen)
{
	if (Data()->RefCount > 1)
	{ // If more than one reference, we must use a new copy
		FStringData *old = Data();
		AllocBuffer (newlen);
		StrCopy (Chars, old->Chars(), newlen < old->Len ? newlen : old->Len);
		old->Release();
	}
	else
	{
		if (newlen > Data()->AllocLen)
		{
			Chars = (char *)(Data()->Realloc(newlen) + 1);
		}
		Data()->Len = (unsigned int)newlen;
	}
}

TArray<FString> FString::Split(const FString &delimiter, const EmptyTokenType keepEmpty) const
{
	return Split(delimiter.GetChars(), keepEmpty);
}

TArray<FString> FString::Split(const char *const delimiter, const EmptyTokenType keepEmpty) const
{
	TArray<FString> tokens;
	Split(tokens, delimiter, keepEmpty);
	return tokens;
}

void FString::Split(TArray<FString>& tokens, const FString &delimiter, EmptyTokenType keepEmpty) const
{
	Split(tokens, delimiter.GetChars(), keepEmpty);
}

void FString::Split(TArray<FString>& tokens, const char *delimiter, EmptyTokenType keepEmpty) const
{
	assert(nullptr != delimiter);

	const long selfLen = static_cast<long>(Len());
	const long delimLen = static_cast<long>(strlen(delimiter));
	long lastPos = 0;

	while (lastPos <= selfLen)
	{
		long pos = IndexOf(delimiter, lastPos);

		if (-1 == pos)
		{
			pos = selfLen;
		}

		if (pos != lastPos || TOK_KEEPEMPTY == keepEmpty)
		{
			tokens.Push(FString(GetChars() + lastPos, pos - lastPos));
		}

		lastPos = pos + delimLen;
	}
}

// Under Windows, use the system heap functions for managing string memory.
// Under other OSs, use ordinary memory management instead.

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE StringHeap;
const SIZE_T STRING_HEAP_SIZE = 64*1024;
#endif

FStringData *FStringData::Alloc (size_t strlen)
{
	strlen += 1 + sizeof(FStringData);	// Add space for header and terminating null
	strlen = (strlen + 7) & ~7;			// Pad length up

#ifdef _WIN32
	if (StringHeap == NULL)
	{
		StringHeap = HeapCreate (0, STRING_HEAP_SIZE, 0);
		if (StringHeap == NULL)
		{
			throw std::bad_alloc();
		}
	}

	FStringData *block = (FStringData *)HeapAlloc (StringHeap, 0, strlen);
#else
	FStringData *block = (FStringData *)malloc (strlen);
#endif
	if (block == NULL)
	{
		throw std::bad_alloc();
	}
	block->Len = 0;
	block->AllocLen = (unsigned int)strlen - sizeof(FStringData) - 1;
	block->RefCount = 1;
	return block;
}

FStringData *FStringData::Realloc (size_t newstrlen)
{
	assert (RefCount <= 1);

	newstrlen += 1 + sizeof(FStringData);	// Add space for header and terminating null
	newstrlen = (newstrlen + 7) & ~7;		// Pad length up

#ifdef _WIN32
	FStringData *block = (FStringData *)HeapReAlloc (StringHeap, 0, this, newstrlen);
#else
	FStringData *block = (FStringData *)realloc (this, newstrlen);
#endif
	if (block == NULL)
	{
		throw std::bad_alloc();
	}
	block->AllocLen = (unsigned int)newstrlen - sizeof(FStringData) - 1;
	return block;
}

void FStringData::Dealloc ()
{
	assert (RefCount <= 0);

#ifdef _WIN32
	HeapFree (StringHeap, 0, this);
#else
	free (this);
#endif
}

FStringData *FStringData::MakeCopy ()
{
	FStringData *copy = Alloc (Len);
	copy->Len = Len;
	FString::StrCopy (copy->Chars(), Chars(), Len);
	return copy;
}

FStringf::FStringf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	VFormat(fmt, ap);
	va_end(ap);
}
