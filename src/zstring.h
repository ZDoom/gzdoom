#ifndef ZSTRING_H
#define ZSTRING_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "tarray.h"

//#define NOPOOLS
class FString
{
public:
	FString () : Chars(NULL) {}

	// Copy constructors
	FString (const FString &other) { Chars = NULL; *this = other; }
	FString (const char *copyStr);
	FString (const char *copyStr, size_t copyLen);
	FString (char oneChar);

	// Concatenation constructors
	FString (const FString &head, const FString &tail);
	FString (const FString &head, const char *tail);
	FString (const FString &head, char tail);
	FString (const char *head, const FString &tail);
	FString (const char *head, const char *tail);
	FString (char head, const FString &tail);

	~FString ();

	operator char *() { return Chars; }
	operator const char *() const { return Chars; }

	char *GetChars() const { return Chars; }
	char &operator[] (int index) { return Chars[index]; }
	char &operator[] (size_t index) { return Chars[index]; }

	FString &operator = (const FString &other);
	FString &operator = (const char *copyStr);

	FString operator + (const FString &tail) const;
	FString operator + (const char *tail) const;
	FString operator + (char tail) const;
	friend FString operator + (const char *head, const FString &tail);
	friend FString operator + (char head, const FString &tail);

	FString &operator += (const FString &tail);
	FString &operator += (const char *tail);
	FString &operator += (char tail);

	FString Left (size_t numChars) const;
	FString Right (size_t numChars) const;
	FString Mid (size_t pos, size_t numChars) const;

	long IndexOf (const FString &substr, long startIndex=0) const;
	long IndexOf (const char *substr, long startIndex=0) const;
	long IndexOf (char subchar, long startIndex=0) const;

	long IndexOfAny (const FString &charset, long startIndex=0) const;
	long IndexOfAny (const char *charset, long startIndex=0) const;

	long LastIndexOf (const FString &substr) const;
	long LastIndexOf (const char *substr) const;
	long LastIndexOf (char subchar) const;
	long LastIndexOf (const FString &substr, long endIndex) const;
	long LastIndexOf (const char *substr, long endIndex) const;
	long LastIndexOf (char subchar, long endIndex) const;
	long LastIndexOf (const char *substr, long endIndex, size_t substrlen) const;

	long LastIndexOfAny (const FString &charset) const;
	long LastIndexOfAny (const char *charset) const;
	long LastIndexOfAny (const FString &charset, long endIndex) const;
	long LastIndexOfAny (const char *charset, long endIndex) const;

	void ToUpper ();
	void ToLower ();
	void SwapCase ();

	void StripLeft ();
	void StripLeft (const FString &charset);
	void StripLeft (const char *charset);

	void StripRight ();
	void StripRight (const FString &charset);
	void StripRight (const char *charset);

	void StripLeftRight ();
	void StripLeftRight (const FString &charset);
	void StripLeftRight (const char *charset);

	void Insert (size_t index, const FString &instr);
	void Insert (size_t index, const char *instr);
	void Insert (size_t index, const char *instr, size_t instrlen);

	void ReplaceChars (char oldchar, char newchar);
	void ReplaceChars (const char *oldcharset, char newchar);

	void StripChars (char killchar);
	void StripChars (const char *killchars);

	void MergeChars (char merger);
	void MergeChars (char merger, char newchar);
	void MergeChars (const char *charset, char newchar);

	void Substitute (const FString &oldstr, const FString &newstr);
	void Substitute (const char *oldstr, const FString &newstr);
	void Substitute (const FString &oldstr, const char *newstr);
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

	void Resize (long newlen);

	int Compare (const FString &other) const { return strcmp (Chars, other.Chars); }
	int Compare (const char *other) const { return strcmp (Chars, other); }

	int CompareNoCase (const FString &other) const { return stricmp (Chars, other.Chars); }
	int CompareNoCase (const char *other) const { return stricmp (Chars, other); }

protected:
	struct StringHeader
	{
#ifndef NOPOOLS
		FString *Owner;		// FString this char array belongs to
#endif
		size_t Len;			// Length of FString, excluding terminating null
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
		char *Alloc (FString *owner, size_t len);
		char *Realloc (FString *owner, char *chars, size_t newlen);
		void Free (char *chars);
	};

	FString (size_t len);
	StringHeader *GetHeader () const;

	static int FormatHelper (void *data, const char *str, int len);
	static void StrCopy (char *to, const char *from, size_t len);
	static void StrCopy (char *to, const FString &from);
	static PoolGroup Pond;

	char *Chars;

#ifndef __GNUC__
	template<> friend bool NeedsDestructor<FString> () { return true; }

	template<> friend void CopyForTArray<FString> (FString &dst, FString &src)
	{
		// When a TArray is resized, we just need to update the Owner, because
		// the old copy is going to go away very soon. No need to call the
		// destructor, either, because full ownership is transferred to the
		// new FString.
		char *chars = src.Chars;
		dst.Chars = chars;
		if (chars != NULL)
		{
			((FString::StringHeader *)(chars - sizeof(FString::StringHeader)))->Owner = &dst;
		}
	}
	template<> friend void ConstructInTArray<FString> (FString *dst, const FString &src)
	{
		new (dst) FString(src);
	}
	template<> friend void ConstructEmptyInTArray<FString> (FString *dst)
	{
		new (dst) FString;
	}
#else
	template<class FString> friend inline void CopyForTArray (FString &dst, FString &src);
	template<class FString> friend inline void ConstructInTArray (FString *dst, const FString &src);
	template<class FString> friend inline void ConstructEmptyInTArray (FString *dst);
#endif

private:
	void *operator new (size_t size, FString *addr)
	{
		return addr;
	}
	void operator delete (void *, FString *)
	{
	}
};

#ifdef __GNUC__
template<> inline bool NeedsDestructor<FString> () { return true; }
template<> inline void CopyForTArray<FString> (FString &dst, FString &src)
{
	// When a TArray is resized, we just need to update the Owner, because
	// the old copy is going to go away very soon. No need to call the
	// destructor, either, because full ownership is transferred to the
	// new FString.
	char *chars = src.Chars;
	dst.Chars = chars;
	if (chars != NULL)
	{
		((FString::StringHeader *)(chars - sizeof(FString::StringHeader)))->Owner = &dst;
	}
}
template<> inline void ConstructInTArray<FString> (FString *dst, const FString &src)
{
	new (dst) FString(src);
}
template<> inline void ConstructEmptyInTArray<FString> (FString *dst)
{
	new (dst) FString;
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
