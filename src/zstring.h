#ifndef ZSTRING_H
#define ZSTRING_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "tarray.h"

//#define NOPOOLS
class string
{
public:
	string () : Chars(NULL) {}

	// Copy constructors
	string (const string &other) { Chars = NULL; *this = other; }
	string (const char *copyStr);
	string (const char *copyStr, size_t copyLen);
	string (char oneChar);

	// Concatenation constructors
	string (const string &head, const string &tail);
	string (const string &head, const char *tail);
	string (const string &head, char tail);
	string (const char *head, const string &tail);
	string (const char *head, const char *tail);
	string (char head, const string &tail);

	~string ();

	char *GetChars() const { return Chars; }
	char &operator[] (int index) { return Chars[index]; }
	char &operator[] (size_t index) { return Chars[index]; }

	string &operator = (const string &other);
	string &operator = (const char *copyStr);

	string operator + (const string &tail) const;
	string operator + (const char *tail) const;
	string operator + (char tail) const;
	friend string operator + (const char *head, const string &tail);
	friend string operator + (char head, const string &tail);

	string &operator += (const string &tail);
	string &operator += (const char *tail);
	string &operator += (char tail);

	string Left (size_t numChars) const;
	string Right (size_t numChars) const;
	string Mid (size_t pos, size_t numChars) const;

	long IndexOf (const string &substr, long startIndex=0) const;
	long IndexOf (const char *substr, long startIndex=0) const;
	long IndexOf (char subchar, long startIndex=0) const;

	long IndexOfAny (const string &charset, long startIndex=0) const;
	long IndexOfAny (const char *charset, long startIndex=0) const;

	long LastIndexOf (const string &substr) const;
	long LastIndexOf (const char *substr) const;
	long LastIndexOf (char subchar) const;
	long LastIndexOf (const string &substr, long endIndex) const;
	long LastIndexOf (const char *substr, long endIndex) const;
	long LastIndexOf (char subchar, long endIndex) const;
	long LastIndexOf (const char *substr, long endIndex, size_t substrlen) const;

	long LastIndexOfAny (const string &charset) const;
	long LastIndexOfAny (const char *charset) const;
	long LastIndexOfAny (const string &charset, long endIndex) const;
	long LastIndexOfAny (const char *charset, long endIndex) const;

	void ToUpper ();
	void ToLower ();
	void SwapCase ();

	void StripLeft ();
	void StripLeft (const string &charset);
	void StripLeft (const char *charset);

	void StripRight ();
	void StripRight (const string &charset);
	void StripRight (const char *charset);

	void StripLeftRight ();
	void StripLeftRight (const string &charset);
	void StripLeftRight (const char *charset);

	void Insert (size_t index, const string &instr);
	void Insert (size_t index, const char *instr);
	void Insert (size_t index, const char *instr, size_t instrlen);

	void ReplaceChars (char oldchar, char newchar);
	void ReplaceChars (const char *oldcharset, char newchar);

	void StripChars (char killchar);
	void StripChars (const char *killchars);

	void MergeChars (char merger);
	void MergeChars (char merger, char newchar);
	void MergeChars (const char *charset, char newchar);

	void Substitute (const string &oldstr, const string &newstr);
	void Substitute (const char *oldstr, const string &newstr);
	void Substitute (const string &oldstr, const char *newstr);
	void Substitute (const char *oldstr, const char *newstr);
	void Substitute (const char *oldstr, const char *newstr, size_t oldstrlen, size_t newstrlen);

	void Format (const char *fmt, ...);
	void VFormat (const char *fmt, va_list arglist);

	bool IsInt () const;
	bool IsFloat () const;
	long ToLong (int base=0) const;
	unsigned long ToULong (int base=0) const;
	double ToDouble () const;

	size_t Len() const { return Chars == NULL ? 0 : ((StringHeader *)(Chars - sizeof(StringHeader)))->Len; }
	bool IsEmpty() const { return Len() == 0; }

	int Compare (const string &other) const { return strcmp (Chars, other.Chars); }
	int Compare (const char *other) const { return strcmp (Chars, other); }

	int CompareNoCase (const string &other) const { return stricmp (Chars, other.Chars); }
	int CompareNoCase (const char *other) const { return stricmp (Chars, other); }

protected:
	struct StringHeader
	{
#ifndef NOPOOLS
		string *Owner;		// string this char array belongs to
#endif
		size_t Len;			// Length of string, excluding terminating null
	};
	struct Pool;
	struct PoolGroup
	{
#ifndef NOPOOLS
		~PoolGroup ();
		Pool *Pools;
		Pool *FindPool (char *chars) const;
		static StringHeader *GetHeader (char *chars);
#endif
		char *Alloc (string *owner, size_t len);
		char *Realloc (string *owner, char *chars, size_t newlen);
		void Free (char *chars);
	};

	string (size_t len);
	StringHeader *GetHeader () const;

	static int FormatHelper (void *data, const char *str, int len);
	static void StrCopy (char *to, const char *from, size_t len);
	static void StrCopy (char *to, const string &from);
	static PoolGroup Pond;

	char *Chars;

#ifndef __GNUC__
	template<> friend bool NeedsDestructor<string> () { return true; }

	template<> friend void CopyForTArray<string> (string &dst, string &src)
	{
		// When a TArray is resized, we just need to update the Owner, because
		// the old copy is going to go away very soon. No need to call the
		// destructor, either, because full ownership is transferred to the
		// new string.
		char *chars = src.Chars;
		dst.Chars = chars;
		if (chars != NULL)
		{
			((string::StringHeader *)(chars - sizeof(string::StringHeader)))->Owner = &dst;
		}
	}
	template<> friend void ConstructInTArray<string> (string *dst, const string &src)
	{
		new (dst) string(src);
	}
	template<> friend void ConstructEmptyInTArray<string> (string *dst)
	{
		new (dst) string;
	}
#else
	template<class string> friend inline void CopyForTArray (string &dst, string &src);
	template<class string> friend inline void ConstructInTArray (string *dst, const string &src);
	template<class string> friend inline void ConstructEmptyInTArray (string *dst);
#endif

private:
	void *operator new (size_t size, string *addr)
	{
		return addr;
	}
	void operator delete (void *, string *)
	{
	}
};

#ifdef __GNUC__
template<> inline bool NeedsDestructor<string> () { return true; }
template<> inline void CopyForTArray<string> (string &dst, string &src)
{
	// When a TArray is resized, we just need to update the Owner, because
	// the old copy is going to go away very soon. No need to call the
	// destructor, either, because full ownership is transferred to the
	// new string.
	char *chars = src.Chars;
	dst.Chars = chars;
	if (chars != NULL)
	{
		((string::StringHeader *)(chars - sizeof(string::StringHeader)))->Owner = &dst;
	}
}
template<> inline void ConstructInTArray<string> (string *dst, const string &src)
{
	new (dst) string(src);
}
template<> inline void ConstructEmptyInTArray<string> (string *dst)
{
	new (dst) string;
}
#endif

namespace StringFormat
{
	enum
	{
		// Format specification flags
		F_MINUS		= 1,
		F_PLUS		= 2,
		F_ZERO		= 4,
		F_BLANK		= 8,
		F_HASH		= 16,

		F_SIGNED	= 32,
		F_NEGATIVE	= 64,
		F_ZEROVALUE	= 128,

		// Format specification size prefixes
		F_HALFHALF	= 0x1000,	// hh
		F_HALF		= 0x2000,	// h
		F_LONG		= 0x3000,	// l
		F_LONGLONG	= 0x4000,	// ll or I64
		F_BIGI		= 0x5000	// I
	};
	typedef int (*OutputFunc)(void *data, const char *str, int len);

	int VWorker (OutputFunc output, void *outputData, const char *fmt, va_list arglist);
	int Worker (OutputFunc output, void *outputData, const char *fmt, ...);
};

#endif
