/* $Id: token.h 547 2006-05-25 13:40:35Z helly $ */
#ifndef _token_h
#define	_token_h

#include "substr.h"

namespace re2c
{

class Token
{

public:
	Str	text;
	uint	line;

public:
	Token(SubStr, uint);
};

inline Token::Token(SubStr t, uint l) : text(t), line(l)
{
	;
}

} // end namespace re2c

#endif
