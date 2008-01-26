/* $Id: substr.cc 546 2006-05-25 13:40:14Z helly $ */
#include <string.h>
#include <stdlib.h>
#include "substr.h"
#include "globals.h"

#ifndef HAVE_STRNDUP

char *strndup(const char *str, size_t len)
{
	char * ret = (char*)malloc(len + 1);
	
	memcpy(ret, str, len);
	ret[len] = '\0';
	return ret;
}

#endif

namespace re2c
{

void SubStr::out(std::ostream& o) const
{
	o.write(str, len);
}

bool operator==(const SubStr &s1, const SubStr &s2)
{
	return (bool) (s1.len == s2.len && memcmp(s1.str, s2.str, s1.len) == 0);
}

Str::Str(const SubStr& s)
	: SubStr(strndup(s.str, s.len), s.len)
{
	;
}

Str::Str(Str& s)
	: SubStr(s.str, s.len)
{
	s.str = NULL;
	s.len = 0;
}

Str::Str()
	: SubStr((char*) NULL, 0)
{
	;
}


Str::~Str()
{
	if (str) {
		free((void*)str);
	}
	str = (char*) - 1;
	len = (uint) - 1;
}

} // end namespace re2c
