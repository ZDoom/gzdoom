/* $Id: substr.h 530 2006-05-25 13:34:33Z helly $ */
#ifndef _substr_h
#define _substr_h

#include <iostream>
#include <string>
#include "basics.h"

namespace re2c
{

class SubStr
{
public:
	const char * str;
	const char * const org;
	uint         len;

public:
	friend bool operator==(const SubStr &, const SubStr &);
	SubStr(const uchar*, uint);
	SubStr(const char*, uint);
	SubStr(const char*);
	SubStr(const SubStr&);
	virtual ~SubStr();
	void out(std::ostream&) const;
	std::string to_string() const;
	uint ofs() const;

#ifdef PEDANTIC
protected:
	SubStr& operator = (const SubStr& oth);
#endif
};

class Str: public SubStr
{
public:
	Str(const SubStr&);
	Str(Str&);
	Str();
	~Str();
};

inline std::ostream& operator<<(std::ostream& o, const SubStr &s)
{
	s.out(o);
	return o;
}

inline std::ostream& operator<<(std::ostream& o, const SubStr* s)
{
	return o << *s;
}

inline SubStr::SubStr(const uchar *s, uint l)
		: str((char*)s), org((char*)s), len(l)
{ }

inline SubStr::SubStr(const char *s, uint l)
		: str(s), org(s), len(l)
{ }

inline SubStr::SubStr(const char *s)
		: str(s), org(s), len(strlen(s))
{ }

inline SubStr::SubStr(const SubStr &s)
		: str(s.str), org(s.str), len(s.len)
{ }

inline SubStr::~SubStr()
{ }

inline std::string SubStr::to_string() const
{
	return std::string(str, len);
}

inline uint SubStr::ofs() const
{
	return str - org;
}

#ifdef PEDANTIC
inline SubStr& SubStr::operator = (const SubStr& oth)
{
	new(this) SubStr(oth);
	return *this;
}
#endif

} // end namespace re2c

#ifndef HAVE_STRNDUP

char *strndup(const char *str, size_t len);

#endif

#endif
