#ifndef NAME_H
#define NAME_H

#include "zstring.h"
#include "tarray.h"

enum ENamedName
{
#define xx(n) NAME_##n,
#include "namedef.h"
#undef xx
};

class name
{
public:
	name () : Index(0) {}
	name (const char *text) { Index = FindName (text, false); }
	name (const string &text) { Index = FindName (text.GetChars(), false); }
	name (const char *text, bool noCreate) { Index = FindName (text, noCreate); }
	name (const string &text, bool noCreate) { Index = FindName (text.GetChars(), noCreate); }
	name (const name &other) { Index = other.Index; }
	name (ENamedName index) { Index = index; }
 //   ~name () {}	// Names can be added but never removed.

	int GetIndex() const { return Index; }
	operator int() const { return Index; }
	const string &GetText() const { return NameArray[Index].Text; }
	const char *GetChars() const { return NameArray[Index].Text.GetChars(); }

	name &operator = (const char *text) { Index = FindName (text, false); return *this; }
	name &operator = (const string &text) { Index = FindName (text.GetChars(), false); return *this; }
	name &operator = (const name &other) { Index = other.Index; return *this; }
	name &operator = (ENamedName index) { Index = index; return *this; }

	int SetName (const char *text, bool noCreate) { return Index = FindName (text, false); }
	int SetName (const string &text, bool noCreate) { return Index = FindName (text.GetChars(), false); }

	bool IsValidName() const { return (unsigned int)Index < NameArray.Size(); }

	// Note that the comparison operators compare the names' indices, not
	// their text, so they cannot be used to do a lexicographical sort.
	bool operator == (const name &other) const { return Index == other.Index; }
	bool operator != (const name &other) const { return Index != other.Index; }
	bool operator <  (const name &other) const { return Index <  other.Index; }
	bool operator <= (const name &other) const { return Index <= other.Index; }
	bool operator >  (const name &other) const { return Index >  other.Index; }
	bool operator >= (const name &other) const { return Index >= other.Index; }

	bool operator == (ENamedName index) const { return Index == index; }
	bool operator != (ENamedName index) const { return Index != index; }
	bool operator <  (ENamedName index) const { return Index <  index; }
	bool operator <= (ENamedName index) const { return Index <= index; }
	bool operator >  (ENamedName index) const { return Index >  index; }
	bool operator >= (ENamedName index) const { return Index >= index; }

private:
	int Index;

	enum { HASH_SIZE = 256 };

	struct MainName
	{
		MainName (int next);
		MainName (const MainName &other) : Text(other.Text), NextHash(other.NextHash) {}
		MainName () {}
		string Text;
		int NextHash;

		void *operator new (size_t size, MainName *addr)
		{
			return addr;
		}
		void operator delete (void *, MainName *)
		{
		}
	};
	static TArray<MainName> NameArray;
	static int Buckets[HASH_SIZE];
	static int FindName (const char *text, bool noCreate);
	static void InitBuckets ();
	static bool Inited;

#ifndef __GNUC__
	template<> friend bool NeedsDestructor<MainName> () { return true; }

	template<> friend void CopyForTArray<MainName> (MainName &dst, MainName &src)
	{
		dst.NextHash = src.NextHash;
		CopyForTArray (dst.Text, src.Text);
	}
	template<> friend void ConstructInTArray<MainName> (MainName *dst, const MainName &src)
	{
		new (dst) name::MainName(src);
	}
	template<> friend void ConstructEmptyInTArray<MainName> (MainName *dst)
	{
		new (dst) name::MainName;
	}
#else
	template<class MainName> friend inline bool NeedsDestructor ();
	template<class MainName> friend inline void CopyForTArray (MainName &dst, MainName &src);
	template<class MainName> friend inline void ConstructInTArray (MainName *dst, const MainName &src);
	template<class MainName> friend inline void ConstructEmptyInTArray (MainName *dst);
#endif
};

#ifdef __GNUC__
template<> inline bool NeedsDestructor<name::MainName> () { return true; }

template<> inline void CopyForTArray<name::MainName> (name::MainName &dst, name::MainName &src)
{
	dst.NextHash = src.NextHash;
	CopyForTArray (dst.Text, src.Text);
}
template<> inline void ConstructInTArray<name::MainName> (name::MainName *dst, const name::MainName &src)
{
	new (dst) name::MainName(src);
}
template<> inline void ConstructEmptyInTArray<name::MainName> (name::MainName *dst)
{
	new (dst) name::MainName;
}
#endif

#endif
