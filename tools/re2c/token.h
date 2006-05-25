/* $Id: token.h,v 1.4 2004/11/01 04:35:57 nuffer Exp $ */
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
