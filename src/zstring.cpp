#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "zstring.h"


FString::FString (size_t len)
{
	Chars = Pond.Alloc (this, len);
}

FString::FString (const char *copyStr)
{
	size_t len = strlen (copyStr);
	Chars = Pond.Alloc (this, len);
	StrCopy (Chars, copyStr, len);
}

FString::FString (const char *copyStr, size_t len)
{
	Chars = Pond.Alloc (this, len);
	StrCopy (Chars, copyStr, len);
}

FString::FString (char oneChar)
{
	Chars = Pond.Alloc (this, 1);
	Chars[0] = oneChar;
	Chars[1] = '\0';
}

FString::FString (const FString &head, const FString &tail)
{
	size_t len1 = head.Len();
	size_t len2 = tail.Len();
	Chars = Pond.Alloc (this, len1 + len2);
	StrCopy (Chars, head);
	StrCopy (Chars + len1, tail);
}

FString::FString (const FString &head, const char *tail)
{
	size_t len1 = head.Len();
	size_t len2 = strlen (tail);
	Chars = Pond.Alloc (this, len1 + len2);
	StrCopy (Chars, head);
	StrCopy (Chars + len1, tail, len2);
}

FString::FString (const FString &head, char tail)
{
	size_t len1 = head.Len();
	Chars = Pond.Alloc (this, len1 + 1);
	StrCopy (Chars, head);
	Chars[len1] = tail;
	Chars[len1+1] = '\0';
}

FString::FString (const char *head, const FString &tail)
{
	size_t len1 = strlen (head);
	size_t len2 = tail.Len();
	Chars = Pond.Alloc (this, len1 + len2);
	StrCopy (Chars, head, len1);
	StrCopy (Chars + len1, tail);
}

FString::FString (const char *head, const char *tail)
{
	size_t len1 = strlen (head);
	size_t len2 = strlen (tail);
	Chars = Pond.Alloc (this, len1 + len2);
	StrCopy (Chars, head, len1);
	StrCopy (Chars + len1, tail, len2);
}

FString::FString (char head, const FString &tail)
{
	size_t len2 = tail.Len();
	Chars = Pond.Alloc (this, 1 + len2);
	Chars[0] = head;
	StrCopy (Chars + 1, tail);
}

FString::~FString ()
{
	if (Chars != NULL)
	{
		Pond.Free (Chars);
		Chars = NULL;
	}
}

FString &FString::operator = (const FString &other)
{
	if (Chars != NULL)
	{
		Pond.Free (Chars);
	}
	if (other.Chars == NULL)
	{
		Chars = NULL;
	}
	else
	{
		Chars = Pond.Alloc (this, other.GetHeader()->Len);
		StrCopy (Chars, other);
	}
	return *this;
}

FString &FString::operator = (const char *copyStr)
{
	if (Chars != NULL)
	{
		Pond.Free (Chars);
	}
	if (copyStr == NULL)
	{
		Chars = NULL;
	}
	else
	{
		size_t len = strlen (copyStr);
		/*
		if (len == 0)
		{
			Chars = NULL;
		}
		else
		*/
		{
			Chars = Pond.Alloc (this, len);
			StrCopy (Chars, copyStr, len);
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

void FString::VFormat (const char *fmt, va_list arglist)
{
	if (Chars != NULL)
	{
		Pond.Free (Chars);
	}
	Chars = NULL;
	StringFormat::VWorker (FormatHelper, this, fmt, arglist);
}

int FString::FormatHelper (void *data, const char *cstr, int len)
{
	FString *str = (FString *)data;
	size_t len1 = str->Len();
	str->Chars = Pond.Realloc (str, str->Chars, len1 + len);
	StrCopy (str->Chars + len1, cstr, len);
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
	Chars = Pond.Realloc (this, Chars, len1 + len2);
	StrCopy (Chars + len1, tail);
	return *this;
}

FString &FString::operator += (const char *tail)
{
	size_t len1 = Len();
	size_t len2 = strlen(tail);
	Chars = Pond.Realloc (this, Chars, len1 + len2);
	StrCopy (Chars + len1, tail, len2);
	return *this;
}

FString &FString::operator += (char tail)
{
	size_t len1 = Len();
	Chars = Pond.Realloc (this, Chars, len1 + 1);
	Chars[len1] = tail;
	Chars[len1+1] = '\0';
	return *this;
}

void FString::Resize (long newlen)
{
	if (newlen >= 0)
	{
		Chars = Pond.Realloc (this, Chars, newlen);
		Chars[newlen] = '\0';
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
		return FString("");
	}
	if (pos + numChars > len)
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
	size_t max = Len();
	for (size_t i = 0; i < max; ++i)
	{
		Chars[i] = toupper(Chars[i]);
	}
}

void FString::ToLower ()
{
	size_t max = Len();
	for (size_t i = 0; i < max; ++i)
	{
		Chars[i] = tolower(Chars[i]);
	}
}

void FString::SwapCase ()
{
	size_t max = Len();
	for (size_t i = 0; i < max; ++i)
	{
		if (isupper(Chars[i]))
		{
			Chars[i] = tolower(Chars[i]);
		}
		else
		{
			Chars[i] = toupper(Chars[i]);
		}
	}
}

void FString::StripLeft ()
{
	size_t max = Len(), i, j;
	for (i = 0; i < max; ++i)
	{
		if (!isspace(Chars[i]))
			break;
	}
	for (j = 0; i <= max; ++j, ++i)
	{
		Chars[j] = Chars[i];
	}
	Pond.Realloc (this, Chars, j-1);
}

void FString::StripLeft (const FString &charset)
{
	return StripLeft (charset.Chars);
}

void FString::StripLeft (const char *charset)
{
	size_t max = Len(), i, j;
	for (i = 0; i < max; ++i)
	{
		if (!strchr (charset, Chars[i]))
			break;
	}
	for (j = 0; i <= max; ++j, ++i)
	{
		Chars[j] = Chars[i];
	}
	Pond.Realloc (this, Chars, j-1);
}

void FString::StripRight ()
{
	size_t max = Len(), i;
	for (i = max - 1; i-- > 0; )
	{
		if (!isspace(Chars[i]))
			break;
	}
	Chars[i+1] = '\0';
	Pond.Realloc (this, Chars, i+1);
}

void FString::StripRight (const FString &charset)
{
	return StripRight (charset.Chars);
}

void FString::StripRight (const char *charset)
{
	size_t max = Len(), i;
	for (i = max - 1; i-- > 0; )
	{
		if (!strchr (charset, Chars[i]))
			break;
	}
	Chars[i+1] = '\0';
	Pond.Realloc (this, Chars, i+1);
}

void FString::StripLeftRight ()
{
	size_t max = Len(), i, j, k;
	for (i = 0; i < max; ++i)
	{
		if (!isspace(Chars[i]))
			break;
	}
	for (j = max - 1; j >= i; --j)
	{
		if (!isspace(Chars[j]))
			break;
	}
	for (k = 0; i <= j; ++i, ++k)
	{
		Chars[k] = Chars[i];
	}
	Chars[k] = '\0';
	Pond.Realloc (this, Chars, k);
}

void FString::StripLeftRight (const FString &charset)
{
	return StripLeft (charset.Chars);
}

void FString::StripLeftRight (const char *charset)
{
	size_t max = Len(), i, j, k;
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
	for (k = 0; i <= j; ++i, ++k)
	{
		Chars[k] = Chars[i];
	}
	Chars[k] = '\0';
	Pond.Realloc (this, Chars, k);
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
	size_t mylen = Len();
	if (index > mylen)
	{
		index = mylen;
	}
	Pond.Realloc (this, Chars, mylen + instrlen);
	if (index < mylen)
	{
		memmove (Chars + index + instrlen, Chars + index, (mylen - index + 1)*sizeof(char));
	}
	memcpy (Chars + index, instr, instrlen*sizeof(char));
	if (index == mylen)
	{
		Chars[mylen + instrlen] = '\0';
	}
}

void FString::ReplaceChars (char oldchar, char newchar)
{
	size_t i, j;
	for (i = 0, j = Len(); i < j; ++i)
	{
		if (Chars[i] == oldchar)
		{
			Chars[i] = newchar;
		}
	}
}

void FString::ReplaceChars (const char *oldcharset, char newchar)
{
	size_t i, j;
	for (i = 0, j = Len(); i < j; ++i)
	{
		if (strchr (oldcharset, Chars[i]) != NULL)
		{
			Chars[i] = newchar;
		}
	}
}

void FString::StripChars (char killchar)
{
	size_t read, write, mylen;
	for (read = write = 0, mylen = Len(); read < mylen; ++read)
	{
		if (Chars[read] != killchar)
		{
			Chars[write++] = Chars[read];
		}
	}
	Chars[write] = '\0';
	Pond.Realloc (this, Chars, write);
}

void FString::StripChars (const char *killchars)
{
	size_t read, write, mylen;
	for (read = write = 0, mylen = Len(); read < mylen; ++read)
	{
		if (strchr (killchars, Chars[read]) == NULL)
		{
			Chars[write++] = Chars[read];
		}
	}
	Chars[write] = '\0';
	Pond.Realloc (this, Chars, write);
}

void FString::MergeChars (char merger)
{
	MergeChars (merger, merger);
}

void FString::MergeChars (char merger, char newchar)
{
	size_t read, write, mylen;
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
	Pond.Realloc (this, Chars, write);
}

void FString::MergeChars (const char *charset, char newchar)
{
	size_t read, write, mylen;
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
	Pond.Realloc (this, Chars, write);
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
	for (size_t checkpt = 0; checkpt < Len(); )
	{
		char *match = strstr (Chars + checkpt, oldstr);
		size_t len = Len();
		if (match != NULL)
		{
			size_t matchpt = match - Chars;
			if (oldstrlen != newstrlen)
			{
				Pond.Realloc (this, Chars, len + newstrlen - oldstrlen);
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
	while (yych != '\0' && isspace(yych)) { yych = *++YYCURSOR; }

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
	while (yych != '\0' && isspace(yych)) { yych = *++YYCURSOR; }
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
	while (yych != '\0' && isspace(yych)) { yych = *++YYCURSOR; }

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
	while (yych != '\0' && isspace(yych)) { yych = *++YYCURSOR; }
	return yych == '\0';
}

long FString::ToLong (int base) const
{
	return strtol (Chars, NULL, base);
}

unsigned long FString::ToULong (int base) const
{
	return strtoul (Chars, NULL, base);
}

double FString::ToDouble () const
{
	return strtod (Chars, NULL);
}

FString::StringHeader *FString::GetHeader () const
{
	if (Chars == NULL) return NULL;
	return (StringHeader *)(Chars - sizeof(StringHeader));
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
